// native/src/message_codec.cpp
// SPDX-License-Identifier: Apache-2.0
#include "message_codec.h"
#include <sstream>
#include <iomanip>
#include <windows.h>

namespace winget_nc {

// ---------------------------------------------------------------------------
// UTF-16 -> UTF-8 conversion
// ---------------------------------------------------------------------------
std::string ToUtf8(const winrt::hstring& hs) {
  if (hs.empty()) return {};
  const int sz = ::WideCharToMultiByte(CP_UTF8, 0,
                                        hs.c_str(), static_cast<int>(hs.size()),
                                        nullptr, 0, nullptr, nullptr);
  std::string out(sz, '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, hs.c_str(), static_cast<int>(hs.size()),
                        out.data(), sz, nullptr, nullptr);
  return out;
}

// ---------------------------------------------------------------------------
// JSON string escaping
// ---------------------------------------------------------------------------
std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (unsigned char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// PostToDart
// ---------------------------------------------------------------------------
bool PostToDart(Dart_Port port, const std::string& json) {
  Dart_CObject obj{};
  obj.type = Dart_CObject_kString;
  // Dart_PostCObject_DL copies the string before returning.
  obj.value.as_string = const_cast<char*>(json.c_str());
  return Dart_PostCObject_DL(port, &obj) == true;
}

// ---------------------------------------------------------------------------
// Package encoder
// ---------------------------------------------------------------------------
std::string EncodePackage(const CatalogPackage& pkg,
                          const std::string& catalog_id,
                          bool include_available_version) {
  auto id      = JsonEscape(ToUtf8(pkg.Id()));
  auto name    = JsonEscape(ToUtf8(pkg.Name()));

  // Installed version (may be null for search-only results)
  std::string version;
  if (auto iv = pkg.InstalledVersion()) {
    version = JsonEscape(ToUtf8(iv.Version()));
  }

  // Available (latest) version
  std::string avail;
  if (include_available_version) {
    if (auto dv = pkg.DefaultInstallVersion()) {
      avail = JsonEscape(ToUtf8(dv.Version()));
    }
  }

  std::string src;
  if (auto dv = pkg.DefaultInstallVersion()) {
    if (auto cat = dv.PackageCatalog()) {
      src = JsonEscape(ToUtf8(cat.Info().Id()));
    }
  }

  std::ostringstream o;
  o << R"({"pkg":{"id":")" << id
    << R"(","name":")" << name
    << R"(","version":")" << (version.empty() ? avail : version) << '"';
  if (include_available_version && !avail.empty()) {
    o << R"(,"available_version":")" << avail << '"';
  }
  o << R"(,"source":")" << src << '"'
    << R"(,"catalog":")" << JsonEscape(catalog_id) << R"("}})";
  return o.str();
}

// ---------------------------------------------------------------------------
// Catalog encoder
// ---------------------------------------------------------------------------
std::string EncodeCatalog(const PackageCatalogInfo& info) {
  std::ostringstream o;
  o << R"({"catalog":{"id":")" << JsonEscape(ToUtf8(info.Id()))
    << R"(","name":")" << JsonEscape(ToUtf8(info.Name())) << R"("}})";
  return o.str();
}

// ---------------------------------------------------------------------------
// Progress encoders
// ---------------------------------------------------------------------------
std::string InstallStateToString(PackageInstallProgressState state) {
  switch (state) {
    case PackageInstallProgressState::Queued:       return "queued";
    case PackageInstallProgressState::Downloading:  return "downloading";
    case PackageInstallProgressState::Installing:   return "installing";
    case PackageInstallProgressState::PostInstall:  return "postInstall";
    case PackageInstallProgressState::Finished:     return "finished";
    default:                                         return "unknown";
  }
}

std::string EncodeInstallProgress(const InstallProgress& p) {
  // WinGet reports downloadProgress (0.0-1.0) and installationProgress
  // (0.0-1.0) separately. Map both into a unified 0-100 percent:
  // downloading phase = 0-50, installing phase = 50-100.
  int pct = 0;
  switch (p.State) {
    case PackageInstallProgressState::Downloading:
      pct = static_cast<int>(p.DownloadProgress * 50.0);
      break;
    case PackageInstallProgressState::Installing:
      pct = 50 + static_cast<int>(p.InstallationProgress * 50.0);
      break;
    case PackageInstallProgressState::Finished:
      pct = 100;
      break;
    default:
      pct = 0;
  }
  const auto state = InstallStateToString(p.State);
  // Derive a human-readable label from state.
  std::string label;
  switch (p.State) {
    case PackageInstallProgressState::Downloading:
      label = "Downloading (" + std::to_string(pct) + "%)";
      break;
    case PackageInstallProgressState::Installing:
      label = "Installing...";
      break;
    case PackageInstallProgressState::PostInstall:
      label = "Finishing up...";
      break;
    case PackageInstallProgressState::Finished:
      label = "Complete";
      break;
    default:
      label = "Pending...";
  }
  std::ostringstream o;
  o << R"({"progress":{"percent":)" << pct
    << R"(,"state":")" << state
    << R"(","label":")" << JsonEscape(label) << R"("}})";
  return o.str();
}

std::string EncodeUninstallProgress(const UninstallProgress& p) {
  int pct = static_cast<int>(p.UninstallationProgress * 100.0);
  std::ostringstream o;
  o << R"({"progress":{"percent":)" << pct
    << R"(,"state":"uninstalling","label":"Uninstalling )" << pct << R"(%"}})";
  return o.str();
}

// ---------------------------------------------------------------------------
// Plan encoder
// ---------------------------------------------------------------------------
static std::string PackageArrayJson(const std::vector<CatalogPackage>& pkgs,
                                    const std::string& catalog_id) {
  std::string out = "[";
  for (size_t i = 0; i < pkgs.size(); ++i) {
    // Strip outer {"pkg":{...}} wrapper — embed inner object directly in array.
    auto full = EncodePackage(pkgs[i], catalog_id);
    // full = {"pkg":{...}} -> extract inner object
    auto inner_start = full.find('{', 1);
    auto inner_end   = full.rfind('}');
    out += full.substr(inner_start, inner_end - inner_start + 1);
    if (i + 1 < pkgs.size()) out += ',';
  }
  out += ']';
  return out;
}

std::string EncodePlan(const InstallPlan& plan, const std::string& catalog_id) {
  std::ostringstream o;
  o << R"({"plan":{"installing":)"  << PackageArrayJson(plan.installing, catalog_id)
    << R"(,"upgrading":)"           << PackageArrayJson(plan.upgrading,  catalog_id)
    << R"(,"removing":)"            << PackageArrayJson(plan.removing,   catalog_id)
    << "}}";
  return o.str();
}

// ---------------------------------------------------------------------------
// Terminal encoders
// ---------------------------------------------------------------------------
std::string EncodeDone()     { return R"({"done":true})"; }
std::string EncodeSuccess()  { return R"({"result":{"success":true}})"; }
std::string EncodeCancelled(){ return R"({"cancelled":true})"; }

std::string EncodeError(const std::string& message, int32_t hresult) {
  std::ostringstream o;
  o << R"({"error":")" << JsonEscape(message) << R"(","hresult":)" << hresult << '}';
  return o.str();
}

std::string EncodeWinrtError(const winrt::hresult_error& e) {
  return EncodeError(ToUtf8(e.message()), static_cast<int32_t>(e.code()));
}

}  // namespace winget_nc

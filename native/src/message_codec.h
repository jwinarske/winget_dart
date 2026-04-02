// native/src/message_codec.h
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define WINRT_LEAN_AND_MEAN
#include <winrt/Microsoft.Management.Deployment.h>

#include "dart/dart_api_dl.h"

namespace winget_nc {

using namespace winrt::Microsoft::Management::Deployment;

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------

/// Escape a UTF-8 string for JSON embedding (handles \, ", and control chars).
std::string JsonEscape(const std::string& s);

/// Convert a winrt::hstring (UTF-16) to UTF-8.
std::string ToUtf8(const winrt::hstring& s);

/// Post a JSON string to a Dart ReceivePort.
/// Returns false if the port is closed (Dart isolate has exited).
bool PostToDart(Dart_Port port, const std::string& json);

// ---------------------------------------------------------------------------
// Package encoder
// ---------------------------------------------------------------------------

/// Encode a CatalogPackage as {"pkg":{...}}.
/// version:          installed version (or latest available for search results)
/// available_version: non-empty when an upgrade is available
std::string EncodePackage(const CatalogPackage& pkg, const std::string& catalog_id,
                          bool include_available_version = false);

// ---------------------------------------------------------------------------
// Catalog encoder
// ---------------------------------------------------------------------------

/// Encode a PackageCatalogInfo as {"catalog":{...}}.
std::string EncodeCatalog(const PackageCatalogInfo& info);

// ---------------------------------------------------------------------------
// Progress encoder
// ---------------------------------------------------------------------------

/// Map InstallProgress.State -> string label.
std::string InstallStateToString(PackageInstallProgressState state);

/// Encode install/upgrade progress as {"progress":{...}}.
std::string EncodeInstallProgress(const InstallProgress& p);

/// Encode uninstall progress as {"progress":{...}}.
std::string EncodeUninstallProgress(const UninstallProgress& p);

// ---------------------------------------------------------------------------
// Plan encoder (simulate)
// ---------------------------------------------------------------------------

struct InstallPlan {
  std::vector<CatalogPackage> installing;
  std::vector<CatalogPackage> upgrading;
  std::vector<CatalogPackage> removing;
};

/// Encode a dependency resolution plan as {"plan":{...}}.
std::string EncodePlan(const InstallPlan& plan, const std::string& catalog_id);

// ---------------------------------------------------------------------------
// Terminal message encoders
// ---------------------------------------------------------------------------

/// {"done":true}
std::string EncodeDone();

/// {"result":{"success":true}}
std::string EncodeSuccess();

/// {"error":"...","hresult":-N}
std::string EncodeError(const std::string& message, int32_t hresult = 0);

/// {"error":"..."} from a winrt::hresult_error.
std::string EncodeWinrtError(const winrt::hresult_error& e);

/// {"cancelled":true}
std::string EncodeCancelled();

}  // namespace winget_nc

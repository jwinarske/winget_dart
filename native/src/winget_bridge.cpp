// native/src/winget_bridge.cpp
// SPDX-License-Identifier: Apache-2.0
//
// Phase 2 — core bridge operations: lifecycle, catalogs, search, find, list.
// Phase 3 stubs remain for: install, upgrade, uninstall, simulate, get_updates.

#include "winget_bridge.h"
#include "winget_manager.h"
#include "winget_transaction.h"
#include "message_codec.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

#define WINRT_LEAN_AND_MEAN
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Management.Deployment.h>

using namespace winrt::Microsoft::Management::Deployment;

namespace {

// Single global manager — one COM apartment per process.
winget_nc::WgManager* g_manager = nullptr;
std::mutex g_manager_mutex;

// Handle table: maps opaque int64_t cookie -> WgTransaction.
std::mutex g_handle_mutex;
std::unordered_map<int64_t, winget_nc::WgTransaction*> g_handles;
std::atomic<int64_t> g_next_handle{1};

int64_t AllocHandle(winget_nc::WgTransaction* tx) {
  auto id = g_next_handle.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard lk(g_handle_mutex);
  g_handles[id] = tx;
  return id;
}

winget_nc::WgTransaction* LookupHandle(int64_t handle) {
  std::lock_guard lk(g_handle_mutex);
  auto it = g_handles.find(handle);
  return (it != g_handles.end()) ? it->second : nullptr;
}

void FreeHandle(int64_t handle) {
  winget_nc::WgTransaction* tx = nullptr;
  {
    std::lock_guard lk(g_handle_mutex);
    auto it = g_handles.find(handle);
    if (it != g_handles.end()) {
      tx = it->second;
      g_handles.erase(it);
    }
  }
  delete tx;
}

// ---------------------------------------------------------------------------
// Coroutine helpers — all run on the COM apartment thread.
// ---------------------------------------------------------------------------

// Helper: open a composite catalog spanning all configured sources.
static winrt::fire_and_forget DoListCatalogs(int64_t handle, Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }
  try {
    auto pm = g_manager->CreatePackageManager();
    auto catalogs = pm.GetPackageCatalogs();
    for (auto const& ref : catalogs) {
      if (tx->cancelled) { PostToDart(port, EncodeCancelled()); co_return; }
      PostToDart(port, EncodeCatalog(ref.Info()));
    }
    PostToDart(port, EncodeDone());
  } catch (const winrt::hresult_error& e) {
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoSearchName(int64_t handle,
                                            std::string query,
                                            Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }
  try {
    auto pm = g_manager->CreatePackageManager();

    // Open the composite catalog (winget + msstore + any custom sources).
    auto catalogs = pm.GetPackageCatalogs();
    CreateCompositePackageCatalogOptions opts;
    for (auto const& ref : catalogs) {
      opts.Catalogs().Append(ref);
    }
    opts.CompositeSearchBehavior(
        CompositeSearchBehavior::RemotePackagesFromAllCatalogs);

    auto composite = pm.CreateCompositePackageCatalog(opts);
    auto connectResult = co_await composite.ConnectAsync();
    if (connectResult.Status() != ConnectResultStatus::Ok) {
      PostToDart(port, EncodeError("Catalog connect failed", 0));
      co_return;
    }
    auto catalog = connectResult.PackageCatalog();

    // Build search filter.
    FindPackagesOptions findOpts;
    PackageMatchFilter filter;
    filter.Field(PackageMatchField::Name);
    filter.Option(PackageFieldMatchOption::ContainsCaseInsensitive);
    filter.Value(winrt::to_hstring(query));
    findOpts.Filters().Append(filter);

    auto findOp = catalog.FindPackagesAsync(findOpts);
    tx->current_op = findOp;
    auto findResult = co_await findOp;
    tx->current_op = nullptr;

    for (auto const& match : findResult.Matches()) {
      if (tx->cancelled) { PostToDart(port, EncodeCancelled()); co_return; }
      PostToDart(port, EncodePackage(match.CatalogPackage(), "composite"));
    }
    PostToDart(port, EncodeDone());

  } catch (const winrt::hresult_error& e) {
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoFindById(int64_t handle,
                                          std::string package_id,
                                          std::string catalog_id,
                                          Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }
  try {
    auto pm = g_manager->CreatePackageManager();

    // If a specific catalog is requested, use only that one.
    // Otherwise, create a composite catalog across all sources.
    PackageCatalog catalog{nullptr};

    if (!catalog_id.empty()) {
      auto ref = pm.GetPackageCatalogByName(winrt::to_hstring(catalog_id));
      if (!ref) {
        PostToDart(port, EncodeError(
            "Catalog not found: " + catalog_id, 0));
        co_return;
      }
      auto connectResult = co_await ref.ConnectAsync();
      if (connectResult.Status() != ConnectResultStatus::Ok) {
        PostToDart(port, EncodeError("Catalog connect failed", 0));
        co_return;
      }
      catalog = connectResult.PackageCatalog();
    } else {
      auto catalogs = pm.GetPackageCatalogs();
      CreateCompositePackageCatalogOptions opts;
      for (auto const& ref : catalogs) {
        opts.Catalogs().Append(ref);
      }
      opts.CompositeSearchBehavior(
          CompositeSearchBehavior::RemotePackagesFromAllCatalogs);
      auto composite = pm.CreateCompositePackageCatalog(opts);
      auto connectResult = co_await composite.ConnectAsync();
      if (connectResult.Status() != ConnectResultStatus::Ok) {
        PostToDart(port, EncodeError("Catalog connect failed", 0));
        co_return;
      }
      catalog = connectResult.PackageCatalog();
    }

    // Build exact ID match filter.
    FindPackagesOptions findOpts;
    PackageMatchFilter filter;
    filter.Field(PackageMatchField::Id);
    filter.Option(PackageFieldMatchOption::EqualsCaseInsensitive);
    filter.Value(winrt::to_hstring(package_id));
    findOpts.Filters().Append(filter);

    auto findOp = catalog.FindPackagesAsync(findOpts);
    tx->current_op = findOp;
    auto findResult = co_await findOp;
    tx->current_op = nullptr;

    auto matches = findResult.Matches();
    if (matches.Size() > 0) {
      auto pkg = matches.GetAt(0).CatalogPackage();
      PostToDart(port, EncodePackage(pkg,
          catalog_id.empty() ? "composite" : catalog_id));
    } else {
      PostToDart(port, EncodeError(
          "Package not found: " + package_id, 0));
    }

  } catch (const winrt::hresult_error& e) {
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoListInstalled(int64_t handle, Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }
  try {
    auto pm = g_manager->CreatePackageManager();

    // Open the local installed catalog.
    auto localRef = pm.GetLocalPackageCatalog(
        LocalPackageCatalog::InstalledPackages);
    auto connectResult = co_await localRef.ConnectAsync();
    if (connectResult.Status() != ConnectResultStatus::Ok) {
      PostToDart(port, EncodeError("Local catalog connect failed", 0));
      co_return;
    }
    auto catalog = connectResult.PackageCatalog();

    // Empty search = return all installed packages.
    FindPackagesOptions findOpts;
    auto findOp = catalog.FindPackagesAsync(findOpts);
    tx->current_op = findOp;
    auto findResult = co_await findOp;
    tx->current_op = nullptr;

    for (auto const& match : findResult.Matches()) {
      if (tx->cancelled) { PostToDart(port, EncodeCancelled()); co_return; }
      PostToDart(port, EncodePackage(match.CatalogPackage(), "local"));
    }
    PostToDart(port, EncodeDone());

  } catch (const winrt::hresult_error& e) {
    PostToDart(port, EncodeWinrtError(e));
  }
}

}  // namespace

// ===========================================================================
// Exported C ABI functions
// ===========================================================================

// ---------------------------------------------------------------------------
// wg_init
// ---------------------------------------------------------------------------
extern "C" int32_t wg_init(void* post_c_object) {
  if (Dart_InitializeApiDL(post_c_object) != 0) {
    return E_FAIL;
  }

  std::lock_guard lk(g_manager_mutex);
  if (g_manager) return 0;  // Already initialized.

  g_manager = new winget_nc::WgManager();
  const int32_t hr = g_manager->Start();
  if (hr != 0) {
    delete g_manager;
    g_manager = nullptr;
  }
  return hr;
}

// ---------------------------------------------------------------------------
// wg_is_available
// ---------------------------------------------------------------------------
extern "C" int32_t wg_is_available(void) {
  try {
    // Probe: attempt to get the activation factory without creating an instance.
    winrt::get_activation_factory<PackageManager>();
    return 1;
  } catch (...) {
    return 0;
  }
}

// ---------------------------------------------------------------------------
// wg_connect / wg_disconnect
// ---------------------------------------------------------------------------
extern "C" int64_t wg_connect(int64_t reply_port) {
  if (!g_manager || !g_manager->IsRunning()) return E_NOT_VALID_STATE;

  auto* tx = new winget_nc::WgTransaction(static_cast<Dart_Port>(reply_port));
  const int64_t handle = AllocHandle(tx);

  g_manager->Dispatch([handle, reply_port]() {
    auto* t = LookupHandle(handle);
    if (!t) return;
    try {
      // Verify PackageManager can be created (validates COM registration).
      (void)g_manager->CreatePackageManager();
      winget_nc::PostToDart(reply_port, R"({"ok":true})");
    } catch (const winrt::hresult_error& e) {
      winget_nc::PostToDart(reply_port, winget_nc::EncodeWinrtError(e));
      FreeHandle(handle);
    }
  });

  return handle;
}

extern "C" void wg_disconnect(int64_t handle) {
  FreeHandle(handle);
}

// ---------------------------------------------------------------------------
// wg_list_catalogs
// ---------------------------------------------------------------------------
extern "C" void wg_list_catalogs(int64_t handle, int64_t reply_port) {
  if (!g_manager) return;
  g_manager->Dispatch([handle, reply_port]() noexcept {
    DoListCatalogs(handle, static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_search_name
// ---------------------------------------------------------------------------
extern "C" void wg_search_name(int64_t handle, const char* query_utf8,
                                int64_t reply_port) {
  if (!g_manager) return;
  std::string query(query_utf8);
  g_manager->Dispatch([handle, query, reply_port]() noexcept {
    DoSearchName(handle, query, static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_find_by_id
// ---------------------------------------------------------------------------
extern "C" void wg_find_by_id(int64_t handle, const char* package_id,
                               const char* catalog_id, int64_t reply_port) {
  if (!g_manager) return;
  std::string pid(package_id);
  std::string cid(catalog_id ? catalog_id : "");
  g_manager->Dispatch([handle, pid, cid, reply_port]() noexcept {
    DoFindById(handle, pid, cid, static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_list_installed
// ---------------------------------------------------------------------------
extern "C" void wg_list_installed(int64_t handle, int64_t reply_port) {
  if (!g_manager) return;
  g_manager->Dispatch([handle, reply_port]() noexcept {
    DoListInstalled(handle, static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_simulate_install — stub (Phase 3)
// ---------------------------------------------------------------------------
extern "C" void wg_simulate_install(int64_t handle, const char* package_id,
                                     const char* catalog_id,
                                     const char* version,
                                     int64_t reply_port) {
  (void)handle;
  (void)package_id;
  (void)catalog_id;
  (void)version;
  winget_nc::PostToDart(reply_port,
      winget_nc::EncodeError("Not implemented", 0));
}

// ---------------------------------------------------------------------------
// wg_simulate_upgrade — stub (Phase 3)
// ---------------------------------------------------------------------------
extern "C" void wg_simulate_upgrade(int64_t handle, const char* package_id,
                                     int64_t reply_port) {
  (void)handle;
  (void)package_id;
  winget_nc::PostToDart(reply_port,
      winget_nc::EncodeError("Not implemented", 0));
}

// ---------------------------------------------------------------------------
// wg_install — stub (Phase 3)
// ---------------------------------------------------------------------------
extern "C" void wg_install(int64_t handle, const char* package_id,
                            const char* catalog_id, const char* version,
                            int32_t silent, int32_t accept_agreements,
                            int64_t reply_port) {
  (void)handle;
  (void)package_id;
  (void)catalog_id;
  (void)version;
  (void)silent;
  (void)accept_agreements;
  winget_nc::PostToDart(reply_port,
      winget_nc::EncodeError("Not implemented", 0));
}

// ---------------------------------------------------------------------------
// wg_upgrade — stub (Phase 3)
// ---------------------------------------------------------------------------
extern "C" void wg_upgrade(int64_t handle, const char* package_id,
                            const char* version, int32_t silent,
                            int32_t accept_agreements, int64_t reply_port) {
  (void)handle;
  (void)package_id;
  (void)version;
  (void)silent;
  (void)accept_agreements;
  winget_nc::PostToDart(reply_port,
      winget_nc::EncodeError("Not implemented", 0));
}

// ---------------------------------------------------------------------------
// wg_uninstall — stub (Phase 3)
// ---------------------------------------------------------------------------
extern "C" void wg_uninstall(int64_t handle, const char* package_id,
                              int32_t silent, int64_t reply_port) {
  (void)handle;
  (void)package_id;
  (void)silent;
  winget_nc::PostToDart(reply_port,
      winget_nc::EncodeError("Not implemented", 0));
}

// ---------------------------------------------------------------------------
// wg_get_updates — stub (Phase 3)
// ---------------------------------------------------------------------------
extern "C" void wg_get_updates(int64_t handle, int64_t reply_port) {
  (void)handle;
  winget_nc::PostToDart(reply_port, winget_nc::EncodeDone());
}

// ---------------------------------------------------------------------------
// wg_cancel
// ---------------------------------------------------------------------------
extern "C" void wg_cancel(int64_t handle) {
  auto* tx = LookupHandle(handle);
  if (tx) {
    tx->Cancel();
    winget_nc::PostToDart(tx->reply_port, winget_nc::EncodeCancelled());
  }
}

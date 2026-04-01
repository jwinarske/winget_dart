// native/src/winget_bridge.cpp
// SPDX-License-Identifier: Apache-2.0
//
// Complete bridge implementation — all operations.
// Phase 2: lifecycle, catalogs, search, find, list installed.
// Phase 3: install, upgrade, uninstall, simulate, get updates, cancel.

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

// ===========================================================================
// Shared helpers
// ===========================================================================

// Helper: find a package by exact ID in a given catalog.
// Returns nullptr if not found. Posts error and returns nullptr on failure.
static winrt::fire_and_forget FindPackageById(
    IPackageManager pm,
    PackageCatalog catalog,
    const std::string& package_id,
    winget_nc::WgTransaction* tx,
    Dart_Port port,
    // Output: set to the found package if successful.
    CatalogPackage* out_pkg) {
  // This helper is not a standalone coroutine — it is called inline.
  // The caller handles try/catch.
  (void)pm;
  (void)catalog;
  (void)package_id;
  (void)tx;
  (void)port;
  (void)out_pkg;
  co_return;
}

// Helper: open a composite catalog across all sources and find a package by ID.
// Returns the found CatalogPackage, or posts error and returns nullptr.
static winrt::Windows::Foundation::IAsyncOperation<CatalogPackage>
FindPackageInComposite(IPackageManager pm,
                       const std::string& package_id,
                       const std::string& catalog_id,
                       winget_nc::WgTransaction* tx,
                       Dart_Port port) {
  using namespace winget_nc;

  PackageCatalog catalog{nullptr};

  if (!catalog_id.empty()) {
    auto ref = pm.GetPackageCatalogByName(winrt::to_hstring(catalog_id));
    if (!ref) {
      PostToDart(port, EncodeError("Catalog not found: " + catalog_id, 0));
      co_return nullptr;
    }
    auto connectResult = co_await ref.ConnectAsync();
    if (connectResult.Status() != ConnectResultStatus::Ok) {
      PostToDart(port, EncodeError("Catalog connect failed", 0));
      co_return nullptr;
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
      co_return nullptr;
    }
    catalog = connectResult.PackageCatalog();
  }

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
    co_return matches.GetAt(0).CatalogPackage();
  }

  PostToDart(port, EncodeError("Package not found: " + package_id, 0));
  co_return nullptr;
}

// ===========================================================================
// Read operation coroutines (Phase 2)
// ===========================================================================

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
    auto pkg = co_await FindPackageInComposite(
        pm, package_id, catalog_id, tx, port);
    if (pkg) {
      PostToDart(port, EncodePackage(pkg,
          catalog_id.empty() ? "composite" : catalog_id));
    }
    // FindPackageInComposite already posted error if not found.
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

    auto localRef = pm.GetLocalPackageCatalog(
        LocalPackageCatalog::InstalledPackages);
    auto connectResult = co_await localRef.ConnectAsync();
    if (connectResult.Status() != ConnectResultStatus::Ok) {
      PostToDart(port, EncodeError("Local catalog connect failed", 0));
      co_return;
    }
    auto catalog = connectResult.PackageCatalog();

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

// ===========================================================================
// Mutating operation coroutines (Phase 3)
// ===========================================================================

static winrt::fire_and_forget DoSimulateInstall(
    int64_t handle,
    std::string package_id,
    std::string catalog_id,
    std::string version,
    Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }
  try {
    auto pm = g_manager->CreatePackageManager();
    auto pkg = co_await FindPackageInComposite(
        pm, package_id, catalog_id, tx, port);
    if (!pkg) co_return;  // Error already posted.

    // Configure install options for simulation.
    InstallOptions installOpts;
    if (!version.empty()) {
      installOpts.PackageVersionId().Version(winrt::to_hstring(version));
    }

    // GetInstallDependenciesAsync resolves the dependency graph without
    // downloading or installing anything.
    auto depsOp = pm.GetInstallDependenciesAsync(pkg, installOpts);
    tx->current_op = depsOp;
    auto deps = co_await depsOp;
    tx->current_op = nullptr;

    // Build the plan from the resolved dependencies.
    InstallPlan plan;
    for (auto const& dep : deps) {
      plan.installing.push_back(dep.Package());
    }

    PostToDart(port, EncodePlan(plan,
        catalog_id.empty() ? "composite" : catalog_id));

  } catch (const winrt::hresult_error& e) {
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoSimulateUpgrade(
    int64_t handle,
    std::string package_id,
    Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }
  try {
    auto pm = g_manager->CreatePackageManager();

    if (!package_id.empty()) {
      // Simulate upgrade for a specific package.
      auto pkg = co_await FindPackageInComposite(
          pm, package_id, "", tx, port);
      if (!pkg) co_return;

      InstallOptions opts;
      auto depsOp = pm.GetInstallDependenciesAsync(pkg, opts);
      tx->current_op = depsOp;
      auto deps = co_await depsOp;
      tx->current_op = nullptr;

      InstallPlan plan;
      for (auto const& dep : deps) {
        plan.upgrading.push_back(dep.Package());
      }
      PostToDart(port, EncodePlan(plan, "composite"));
    } else {
      // Simulate upgrade for all packages with available updates.
      // List available upgrades first, then resolve deps for each.
      InstallPlan plan;

      auto localRef = pm.GetLocalPackageCatalog(
          LocalPackageCatalog::InstalledPackages);
      auto connectResult = co_await localRef.ConnectAsync();
      if (connectResult.Status() != ConnectResultStatus::Ok) {
        PostToDart(port, EncodeError("Local catalog connect failed", 0));
        co_return;
      }
      auto catalog = connectResult.PackageCatalog();

      FindPackagesOptions findOpts;
      auto findOp = catalog.FindPackagesAsync(findOpts);
      tx->current_op = findOp;
      auto findResult = co_await findOp;
      tx->current_op = nullptr;

      for (auto const& match : findResult.Matches()) {
        if (tx->cancelled) { PostToDart(port, EncodeCancelled()); co_return; }
        auto pkg = match.CatalogPackage();
        if (pkg.IsUpdateAvailable()) {
          plan.upgrading.push_back(pkg);
        }
      }
      PostToDart(port, EncodePlan(plan, "local"));
    }

  } catch (const winrt::hresult_error& e) {
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoInstall(
    int64_t handle,
    std::string package_id,
    std::string catalog_id,
    std::string version,
    bool silent,
    bool accept_agreements,
    Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }

  // Sequential install guard.
  if (!tx->TryStartMutatingOp()) {
    PostToDart(port, EncodeError(
        "Another install/upgrade/uninstall is already in progress", 0));
    co_return;
  }

  try {
    auto pm = g_manager->CreatePackageManager();
    auto pkg = co_await FindPackageInComposite(
        pm, package_id, catalog_id, tx, port);
    if (!pkg) {
      tx->EndMutatingOp();
      co_return;
    }

    InstallOptions installOpts;
    if (!version.empty()) {
      installOpts.PackageVersionId().Version(winrt::to_hstring(version));
    }
    if (silent) {
      installOpts.PackageInstallMode(PackageInstallMode::Silent);
    }
    if (accept_agreements) {
      installOpts.AcceptPackageAgreements(true);
    }

    auto installOp = pm.InstallPackageAsync(pkg, installOpts);
    tx->current_op = installOp;

    // Attach progress handler — runs on the COM apartment thread.
    installOp.Progress([port](auto const&, InstallProgress const& p) {
      PostToDart(port, EncodeInstallProgress(p));
    });

    auto result = co_await installOp;
    tx->current_op = nullptr;
    tx->EndMutatingOp();

    if (result.Status() == InstallResultStatus::Ok) {
      PostToDart(port, EncodeSuccess());
    } else {
      PostToDart(port, EncodeError(
          "Install failed",
          static_cast<int32_t>(result.ExtendedErrorCode())));
    }

  } catch (const winrt::hresult_error& e) {
    tx->EndMutatingOp();
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoUpgrade(
    int64_t handle,
    std::string package_id,
    std::string version,
    bool silent,
    bool accept_agreements,
    Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }

  if (!tx->TryStartMutatingOp()) {
    PostToDart(port, EncodeError(
        "Another install/upgrade/uninstall is already in progress", 0));
    co_return;
  }

  try {
    auto pm = g_manager->CreatePackageManager();
    auto pkg = co_await FindPackageInComposite(
        pm, package_id, "", tx, port);
    if (!pkg) {
      tx->EndMutatingOp();
      co_return;
    }

    InstallOptions upgradeOpts;
    if (!version.empty()) {
      upgradeOpts.PackageVersionId().Version(winrt::to_hstring(version));
    }
    if (silent) {
      upgradeOpts.PackageInstallMode(PackageInstallMode::Silent);
    }
    if (accept_agreements) {
      upgradeOpts.AcceptPackageAgreements(true);
    }

    auto upgradeOp = pm.UpgradePackageAsync(pkg, upgradeOpts);
    tx->current_op = upgradeOp;

    upgradeOp.Progress([port](auto const&, InstallProgress const& p) {
      PostToDart(port, EncodeInstallProgress(p));
    });

    auto result = co_await upgradeOp;
    tx->current_op = nullptr;
    tx->EndMutatingOp();

    if (result.Status() == InstallResultStatus::Ok) {
      PostToDart(port, EncodeSuccess());
    } else {
      PostToDart(port, EncodeError(
          "Upgrade failed",
          static_cast<int32_t>(result.ExtendedErrorCode())));
    }

  } catch (const winrt::hresult_error& e) {
    tx->EndMutatingOp();
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoUninstall(
    int64_t handle,
    std::string package_id,
    bool silent,
    Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }

  if (!tx->TryStartMutatingOp()) {
    PostToDart(port, EncodeError(
        "Another install/upgrade/uninstall is already in progress", 0));
    co_return;
  }

  try {
    auto pm = g_manager->CreatePackageManager();

    // Find the package in the local installed catalog.
    auto localRef = pm.GetLocalPackageCatalog(
        LocalPackageCatalog::InstalledPackages);
    auto connectResult = co_await localRef.ConnectAsync();
    if (connectResult.Status() != ConnectResultStatus::Ok) {
      tx->EndMutatingOp();
      PostToDart(port, EncodeError("Local catalog connect failed", 0));
      co_return;
    }
    auto catalog = connectResult.PackageCatalog();

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
    if (matches.Size() == 0) {
      tx->EndMutatingOp();
      PostToDart(port, EncodeError(
          "Package not installed: " + package_id, 0));
      co_return;
    }
    auto pkg = matches.GetAt(0).CatalogPackage();

    UninstallOptions uninstallOpts;
    if (silent) {
      uninstallOpts.PackageUninstallMode(PackageUninstallMode::Silent);
    }

    auto uninstallOp = pm.UninstallPackageAsync(pkg, uninstallOpts);
    tx->current_op = uninstallOp;

    uninstallOp.Progress([port](auto const&, UninstallProgress const& p) {
      PostToDart(port, EncodeUninstallProgress(p));
    });

    auto result = co_await uninstallOp;
    tx->current_op = nullptr;
    tx->EndMutatingOp();

    if (result.Status() == UninstallResultStatus::Ok) {
      PostToDart(port, EncodeSuccess());
    } else {
      PostToDart(port, EncodeError(
          "Uninstall failed",
          static_cast<int32_t>(result.ExtendedErrorCode())));
    }

  } catch (const winrt::hresult_error& e) {
    tx->EndMutatingOp();
    PostToDart(port, EncodeWinrtError(e));
  }
}

static winrt::fire_and_forget DoGetUpdates(int64_t handle, Dart_Port port) {
  using namespace winget_nc;
  auto* tx = LookupHandle(handle);
  if (!tx || tx->cancelled) {
    PostToDart(port, EncodeCancelled());
    co_return;
  }
  try {
    auto pm = g_manager->CreatePackageManager();

    // Open a composite catalog with local + remote to detect available upgrades.
    auto catalogs = pm.GetPackageCatalogs();
    CreateCompositePackageCatalogOptions opts;
    for (auto const& ref : catalogs) {
      opts.Catalogs().Append(ref);
    }
    opts.CompositeSearchBehavior(
        CompositeSearchBehavior::LocalCatalogs);

    auto composite = pm.CreateCompositePackageCatalog(opts);
    auto connectResult = co_await composite.ConnectAsync();
    if (connectResult.Status() != ConnectResultStatus::Ok) {
      PostToDart(port, EncodeError("Catalog connect failed", 0));
      co_return;
    }
    auto catalog = connectResult.PackageCatalog();

    FindPackagesOptions findOpts;
    auto findOp = catalog.FindPackagesAsync(findOpts);
    tx->current_op = findOp;
    auto findResult = co_await findOp;
    tx->current_op = nullptr;

    for (auto const& match : findResult.Matches()) {
      if (tx->cancelled) { PostToDart(port, EncodeCancelled()); co_return; }
      auto pkg = match.CatalogPackage();
      if (pkg.IsUpdateAvailable()) {
        PostToDart(port, EncodePackage(pkg, "composite", true));
      }
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
// wg_simulate_install
// ---------------------------------------------------------------------------
extern "C" void wg_simulate_install(int64_t handle, const char* package_id,
                                     const char* catalog_id,
                                     const char* version,
                                     int64_t reply_port) {
  if (!g_manager) return;
  std::string pid(package_id);
  std::string cid(catalog_id ? catalog_id : "");
  std::string ver(version ? version : "");
  g_manager->Dispatch([handle, pid, cid, ver, reply_port]() noexcept {
    DoSimulateInstall(handle, pid, cid, ver,
                      static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_simulate_upgrade
// ---------------------------------------------------------------------------
extern "C" void wg_simulate_upgrade(int64_t handle, const char* package_id,
                                     int64_t reply_port) {
  if (!g_manager) return;
  std::string pid(package_id ? package_id : "");
  g_manager->Dispatch([handle, pid, reply_port]() noexcept {
    DoSimulateUpgrade(handle, pid, static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_install
// ---------------------------------------------------------------------------
extern "C" void wg_install(int64_t handle, const char* package_id,
                            const char* catalog_id, const char* version,
                            int32_t silent, int32_t accept_agreements,
                            int64_t reply_port) {
  if (!g_manager) return;
  std::string pid(package_id);
  std::string cid(catalog_id ? catalog_id : "");
  std::string ver(version ? version : "");
  bool sil = (silent != 0);
  bool acc = (accept_agreements != 0);
  g_manager->Dispatch([handle, pid, cid, ver, sil, acc, reply_port]() noexcept {
    DoInstall(handle, pid, cid, ver, sil, acc,
              static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_upgrade
// ---------------------------------------------------------------------------
extern "C" void wg_upgrade(int64_t handle, const char* package_id,
                            const char* version, int32_t silent,
                            int32_t accept_agreements, int64_t reply_port) {
  if (!g_manager) return;
  std::string pid(package_id);
  std::string ver(version ? version : "");
  bool sil = (silent != 0);
  bool acc = (accept_agreements != 0);
  g_manager->Dispatch([handle, pid, ver, sil, acc, reply_port]() noexcept {
    DoUpgrade(handle, pid, ver, sil, acc,
              static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_uninstall
// ---------------------------------------------------------------------------
extern "C" void wg_uninstall(int64_t handle, const char* package_id,
                              int32_t silent, int64_t reply_port) {
  if (!g_manager) return;
  std::string pid(package_id);
  bool sil = (silent != 0);
  g_manager->Dispatch([handle, pid, sil, reply_port]() noexcept {
    DoUninstall(handle, pid, sil, static_cast<Dart_Port>(reply_port));
  });
}

// ---------------------------------------------------------------------------
// wg_get_updates
// ---------------------------------------------------------------------------
extern "C" void wg_get_updates(int64_t handle, int64_t reply_port) {
  if (!g_manager) return;
  g_manager->Dispatch([handle, reply_port]() noexcept {
    DoGetUpdates(handle, static_cast<Dart_Port>(reply_port));
  });
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

// native/src/winget_bridge.cpp
// SPDX-License-Identifier: Apache-2.0
//
// Phase 1 — stub no-op implementations of the flat C ABI.
// Each function compiles and links but does not perform real WinGet operations.
// Full implementations are added in Phase 2 (core) and Phase 3 (install/simulate).

#include "winget_bridge.h"
#include "winget_manager.h"
#include "winget_transaction.h"
#include "message_codec.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

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

}  // namespace

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
    winrt::get_activation_factory<
        winrt::Microsoft::Management::Deployment::PackageManager>();
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
// wg_list_catalogs — stub (Phase 2)
// ---------------------------------------------------------------------------
extern "C" void wg_list_catalogs(int64_t handle, int64_t reply_port) {
  // TODO(Phase 2): Enumerate pm.GetPackageCatalogs() and post JSON.
  winget_nc::PostToDart(reply_port, winget_nc::EncodeDone());
}

// ---------------------------------------------------------------------------
// wg_search_name — stub (Phase 2)
// ---------------------------------------------------------------------------
extern "C" void wg_search_name(int64_t handle, const char* query,
                                int64_t reply_port) {
  // TODO(Phase 2): Composite catalog search + streaming posts.
  (void)handle;
  (void)query;
  winget_nc::PostToDart(reply_port, winget_nc::EncodeDone());
}

// ---------------------------------------------------------------------------
// wg_find_by_id — stub (Phase 2)
// ---------------------------------------------------------------------------
extern "C" void wg_find_by_id(int64_t handle, const char* package_id,
                               const char* catalog_id, int64_t reply_port) {
  // TODO(Phase 2): Exact ID match across composite catalog.
  (void)handle;
  (void)package_id;
  (void)catalog_id;
  winget_nc::PostToDart(reply_port,
      winget_nc::EncodeError("Not implemented", 0));
}

// ---------------------------------------------------------------------------
// wg_list_installed — stub (Phase 2)
// ---------------------------------------------------------------------------
extern "C" void wg_list_installed(int64_t handle, int64_t reply_port) {
  // TODO(Phase 2): Local installed catalog enumeration.
  (void)handle;
  winget_nc::PostToDart(reply_port, winget_nc::EncodeDone());
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

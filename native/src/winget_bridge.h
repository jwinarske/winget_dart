// native/src/winget_bridge.h
// SPDX-License-Identifier: Apache-2.0
//
// Flat C ABI for winget_dart.
// All strings are UTF-8. All handles are opaque int64_t cookies.
// All async operations post results to Dart via Dart_PostCObject_DL.
// Must be compiled as C++ (uses WinRT internally), exported as C symbols.

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef WINGET_NC_EXPORTS
#define WINGET_NC_API __declspec(dllexport)
#else
#define WINGET_NC_API __declspec(dllimport)
#endif

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/// Initialize the bridge. Must be called once per process before any other
/// function. Passes the Dart VM's NativeApi.postCObject function pointer so
/// the bridge can send events back to Dart from the COM thread.
///
/// @param post_c_object  NativeApi.postCObject from dart:ffi
/// @returns 0 on success, non-zero HRESULT on failure.
WINGET_NC_API int32_t wg_init(void* post_c_object);

/// Connect to the Windows Package Manager COM server.
/// Detects process elevation and selects the appropriate COM factory.
/// Returns an opaque handle (> 0) or a negative HRESULT on failure.
///
/// @param reply_port  Dart ReceivePort.sendPort.nativePort — receives the
///                    connect result: {"ok": true} or {"error": "...", "hresult": N}
WINGET_NC_API int64_t wg_connect(int64_t reply_port);

/// Disconnect and release all COM references. Safe to call from any thread.
WINGET_NC_API void wg_disconnect(int64_t handle);

/// Returns 1 if WinGet App Installer is present and reachable, 0 otherwise.
WINGET_NC_API int32_t wg_is_available(void);

// ---------------------------------------------------------------------------
// Catalogs
// ---------------------------------------------------------------------------

/// List all configured package catalogs (winget, msstore, custom).
/// Posts a JSON array of catalog objects to reply_port:
///   [{"id":"winget","name":"WinGet Community Repository"}, ...]
WINGET_NC_API void wg_list_catalogs(int64_t handle, int64_t reply_port);

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

/// Search for packages by name across all catalogs.
/// Posts a stream of package JSON objects to reply_port, then a sentinel
/// {"done": true} when complete, or {"error": "..."} on failure.
///
/// @param query  UTF-8 search string (name match)
WINGET_NC_API void wg_search_name(int64_t handle, const char* query,
                                  int64_t reply_port);

/// Search for a package by exact ID.
/// Posts {"package": {...}} or {"error": "..."} to reply_port.
WINGET_NC_API void wg_find_by_id(int64_t handle, const char* package_id,
                                 const char* catalog_id, // NULL = all catalogs
                                 int64_t reply_port);

// ---------------------------------------------------------------------------
// Installed packages
// ---------------------------------------------------------------------------

/// List all installed packages.
/// Posts a stream of package JSON objects then {"done": true}.
WINGET_NC_API void wg_list_installed(int64_t handle, int64_t reply_port);

// ---------------------------------------------------------------------------
// Simulate (dry-run dependency resolution)
// ---------------------------------------------------------------------------

/// Simulate install — resolve dependencies without installing anything.
/// Posts {"plan": {"installing": [...], "upgrading": [...], "removing": [...]}}
/// or {"error": "..."} to reply_port.
WINGET_NC_API void wg_simulate_install(int64_t handle, const char* package_id,
                                       const char* catalog_id, // NULL = winget default
                                       const char* version,    // NULL = latest
                                       int64_t reply_port);

/// Simulate upgrade (all packages or a specific one).
WINGET_NC_API void wg_simulate_upgrade(int64_t handle,
                                       const char* package_id, // NULL = all
                                       int64_t reply_port);

// ---------------------------------------------------------------------------
// Install / Upgrade / Uninstall
// ---------------------------------------------------------------------------

/// Install a package.
/// Posts progress events: {"progress": {"percent": N, "state": "...", "label": "..."}}
/// Completes with {"result": {"success": true}} or {"error": "...", "hresult": N}.
///
/// @param silent             Install silently (no installer UI)
/// @param accept_agreements  Auto-accept source/package agreements
WINGET_NC_API void wg_install(int64_t handle, const char* package_id,
                              const char* catalog_id,    // NULL = winget default
                              const char* version,       // NULL = latest
                              int32_t silent,
                              int32_t accept_agreements,
                              int64_t reply_port);

/// Upgrade an installed package to the latest (or specified) version.
WINGET_NC_API void wg_upgrade(int64_t handle, const char* package_id,
                              const char* version, // NULL = latest
                              int32_t silent,
                              int32_t accept_agreements,
                              int64_t reply_port);

/// Uninstall an installed package.
WINGET_NC_API void wg_uninstall(int64_t handle, const char* package_id,
                                int32_t silent, int64_t reply_port);

// ---------------------------------------------------------------------------
// Updates check
// ---------------------------------------------------------------------------

/// List packages that have available upgrades.
/// Posts a stream of package JSON objects then {"done": true}.
WINGET_NC_API void wg_get_updates(int64_t handle, int64_t reply_port);

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

/// Request cancellation of any ongoing operation on the given handle.
/// The operation posts {"cancelled": true} to its reply_port.
WINGET_NC_API void wg_cancel(int64_t handle);

#ifdef __cplusplus
}
#endif

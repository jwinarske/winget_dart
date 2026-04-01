// native/src/winget_manager.h
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#define WINRT_LEAN_AND_MEAN
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Management.Deployment.h>
#include "winget_interop/WindowsPackageManager.h"

#include "dart/dart_api_dl.h"

namespace winget_nc {

using namespace winrt::Microsoft::Management::Deployment;

/// Owns the single MTA COM apartment thread for the process.
/// All WinRT calls must be dispatched to this thread.
/// There is exactly one WgManager per process (singleton created by wg_init).
class WgManager {
 public:
  WgManager() = default;
  ~WgManager();

  // Non-copyable, non-movable — held by raw pointer in the bridge.
  WgManager(const WgManager&) = delete;
  WgManager& operator=(const WgManager&) = delete;

  /// Start the COM apartment thread and block until it is ready.
  /// Returns 0 on success, HRESULT on failure.
  int32_t Start();

  /// Stop the COM apartment thread gracefully.
  void Stop();

  /// Dispatch a callable to the COM apartment thread and return immediately.
  /// The callable is invoked on the apartment thread; use Dart_PostCObject_DL
  /// to send results back to Dart.
  void Dispatch(std::function<void()> fn);

  /// Create a new IPackageManager for a caller.
  /// Returned object is valid only on the apartment thread.
  IPackageManager CreatePackageManager();

  bool IsRunning() const { return running_.load(std::memory_order_acquire); }

 private:
  std::thread           apartment_thread_;
  std::atomic<bool>     running_{false};

  // Startup synchronization: apartment thread signals ready via this CV.
  std::mutex            start_mutex_;
  std::condition_variable start_cv_;
  int32_t               start_hresult_{0};

  // Task queue for Dispatch().
  std::mutex            task_mutex_;
  std::condition_variable task_cv_;
  std::vector<std::function<void()>> task_queue_;

  // Message pump HWND used to wake the thread via PostMessage.
  HWND                  hwnd_{nullptr};

  void ApartmentMain();
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  static bool IsElevated();
};

}  // namespace winget_nc

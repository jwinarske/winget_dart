// native/src/winget_transaction.h
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#define WINRT_LEAN_AND_MEAN
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.Management.Deployment.h>

#include "dart/dart_api_dl.h"

namespace winget_nc {

using namespace winrt::Microsoft::Management::Deployment;

/// Wraps a single WinGet async operation.
/// Holds a reference to the in-flight IAsyncOperation so it can be cancelled.
/// Lives on the COM apartment thread. Created and destroyed by bridge functions.
struct WgTransaction {
  Dart_Port reply_port;

  // The cancellable token for the current async op.
  // Only one of these is non-null at a time.
  winrt::Windows::Foundation::IAsyncInfo current_op{nullptr};

  std::atomic<bool> cancelled{false};

  explicit WgTransaction(Dart_Port port) : reply_port(port) {}

  void Cancel() {
    cancelled.store(true, std::memory_order_release);
    if (current_op) {
      try { current_op.Cancel(); } catch (...) {}
    }
  }
};

}  // namespace winget_nc

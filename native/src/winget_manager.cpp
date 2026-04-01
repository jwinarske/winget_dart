// native/src/winget_manager.cpp
// SPDX-License-Identifier: Apache-2.0
//
// Phase 1 stub — compiles but does not implement COM apartment logic.
// Full implementation is Phase 2.

#include "winget_manager.h"
#include <stdexcept>

namespace winget_nc {

// Custom window message: process next task from queue.
static constexpr UINT WM_DISPATCH_TASK = WM_USER + 1;
// Custom window message: quit the apartment thread.
static constexpr UINT WM_QUIT_APARTMENT = WM_USER + 2;

// ---------------------------------------------------------------------------
// Elevation detection
// ---------------------------------------------------------------------------
bool WgManager::IsElevated() {
  BOOL elevated = FALSE;
  HANDLE token = nullptr;
  if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
    TOKEN_ELEVATION info{};
    DWORD size = sizeof(info);
    ::GetTokenInformation(token, TokenElevation, &info, size, &size);
    elevated = info.TokenIsElevated;
    ::CloseHandle(token);
  }
  return elevated != FALSE;
}

// ---------------------------------------------------------------------------
// Apartment main
// ---------------------------------------------------------------------------
void WgManager::ApartmentMain() {
  // Initialize MTA. WinGet COM server requires multi-threaded apartment.
  winrt::init_apartment(winrt::apartment_type::multi_threaded);

  // Create a message-only window so PostMessage can wake the loop.
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.lpszClassName = L"WingetNcApartment";
  ::RegisterClassExW(&wc);
  hwnd_ = ::CreateWindowExW(0, L"WingetNcApartment", nullptr, 0,
                             0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, this);

  {
    std::lock_guard lk(start_mutex_);
    start_hresult_ = (hwnd_ != nullptr) ? 0 : HRESULT_FROM_WIN32(::GetLastError());
    running_.store(true, std::memory_order_release);
  }
  start_cv_.notify_one();

  if (!hwnd_) {
    winrt::uninit_apartment();
    return;
  }

  // Pump messages until WM_QUIT_APARTMENT.
  MSG msg{};
  while (::GetMessageW(&msg, hwnd_, 0, 0) > 0) {
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);
  }

  ::DestroyWindow(hwnd_);
  hwnd_ = nullptr;
  winrt::uninit_apartment();
  running_.store(false, std::memory_order_release);
}

LRESULT CALLBACK WgManager::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_DISPATCH_TASK) {
    auto* self = reinterpret_cast<WgManager*>(
        ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
      std::vector<std::function<void()>> tasks;
      {
        std::lock_guard lk(self->task_mutex_);
        tasks.swap(self->task_queue_);
      }
      for (auto& fn : tasks) fn();
    }
    return 0;
  }
  if (msg == WM_QUIT_APARTMENT) {
    ::PostQuitMessage(0);
    return 0;
  }
  if (msg == WM_CREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    return 0;
  }
  return ::DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
int32_t WgManager::Start() {
  apartment_thread_ = std::thread([this] { ApartmentMain(); });

  std::unique_lock lk(start_mutex_);
  start_cv_.wait(lk, [this] { return running_.load(); });
  return start_hresult_;
}

void WgManager::Stop() {
  if (hwnd_) ::PostMessageW(hwnd_, WM_QUIT_APARTMENT, 0, 0);
  if (apartment_thread_.joinable()) apartment_thread_.join();
}

WgManager::~WgManager() { Stop(); }

void WgManager::Dispatch(std::function<void()> fn) {
  {
    std::lock_guard lk(task_mutex_);
    task_queue_.push_back(std::move(fn));
  }
  if (hwnd_) ::PostMessageW(hwnd_, WM_DISPATCH_TASK, 0, 0);
}

IPackageManager WgManager::CreatePackageManager() {
  // Factory selection: wrong factory -> hard crash, no exception.
  // Always detect elevation fresh; do not cache.
  if (IsElevated()) {
    WindowsPackageManagerElevatedFactory factory;
    return factory.CreatePackageManager();
  }
  WindowsPackageManagerStandardFactory factory;
  return factory.CreatePackageManager();
}

}  // namespace winget_nc

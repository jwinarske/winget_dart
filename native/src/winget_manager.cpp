// native/src/winget_manager.cpp
// SPDX-License-Identifier: Apache-2.0

#include "winget_manager.h"

#include <Softpub.h>
#include <appmodel.h>
#include <mscat.h>
#include <objbase.h>
#include <roapi.h>
#include <sddl.h>
#include <winstring.h>
#include <wintrust.h>

#include <stdexcept>

#pragma comment(lib, "wintrust.lib")

namespace winget_nc {

// ---------------------------------------------------------------------------
// Activation strategies for WinGet COM objects
// ---------------------------------------------------------------------------
//
// WinGet COM classes require package identity when activated via the standard
// RoGetActivationFactory / CoCreateInstance path. Unpackaged callers (like
// dart.exe) must bypass this. We try strategies in this order:
//
//   1. In-process module from WindowsPackageManager.dll in the App Installer
//      MSIX package. This DLL exports WindowsPackageManagerInProcModule*
//      functions that activate WinGet types entirely in-process without any
//      package identity requirement.
//
//   2. DllGetClassObject from the NuGet interop DLL
//      (Microsoft.Management.Deployment.dll shipped alongside winget_nc.dll).
//
//   3. DllGetActivationFactory from the same NuGet interop DLL.
//
//   4. Standard CoCreateInstance (only works if caller has package identity).
// ---------------------------------------------------------------------------

// --- NuGet interop DLL (OutOfProc proxy) ---

using DllGetActivationFactory_t = HRESULT(WINAPI*)(HSTRING classId, IActivationFactory** factory);
using DllGetClassObject_t = HRESULT(WINAPI*)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);

static HMODULE g_interop_dll = nullptr;
static DllGetActivationFactory_t g_get_factory = nullptr;
static DllGetClassObject_t g_get_class_object = nullptr;

// --- App Installer in-process module ---

using InProcInitialize_t = HRESULT(WINAPI*)();
using InProcTerminate_t = HRESULT(WINAPI*)();
using InProcGetActivationFactory_t = HRESULT(WINAPI*)(HSTRING classId,
                                                      IActivationFactory** factory);
using InProcGetClassObject_t = HRESULT(WINAPI*)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);

static HMODULE g_inproc_dll = nullptr;
static InProcGetActivationFactory_t g_inproc_get_factory = nullptr;
static InProcGetClassObject_t g_inproc_get_class_object = nullptr;
static InProcTerminate_t g_inproc_terminate = nullptr;
static DLL_DIRECTORY_COOKIE g_inproc_dir_cookie = nullptr;

/// Find the App Installer (DesktopAppInstaller) package installation path.
/// Uses GetPackagesByPackageFamily + GetPackagePathByFullName (Win32 appmodel API).
static std::wstring FindAppInstallerPath() {
  const wchar_t* family = L"Microsoft.DesktopAppInstaller_8wekyb3d8bbwe";

  UINT32 count = 0;
  UINT32 bufferLength = 0;
  LONG rc = ::GetPackagesByPackageFamily(family, &count, nullptr, &bufferLength, nullptr);
  if (rc != ERROR_INSUFFICIENT_BUFFER || count == 0) return {};

  std::vector<PWSTR> names(count);
  std::vector<wchar_t> buffer(bufferLength);
  rc = ::GetPackagesByPackageFamily(family, &count, names.data(), &bufferLength, buffer.data());
  if (rc != ERROR_SUCCESS || count == 0) return {};

  // Use the first (usually only) package.
  UINT32 pathLen = 0;
  rc = ::GetPackagePathByFullName(names[0], &pathLen, nullptr);
  if (rc != ERROR_INSUFFICIENT_BUFFER) return {};

  std::wstring path(pathLen, L'\0');
  rc = ::GetPackagePathByFullName(names[0], &pathLen, path.data());
  if (rc != ERROR_SUCCESS) return {};

  // Trim trailing null.
  while (!path.empty() && path.back() == L'\0') path.pop_back();
  return path;
}

/// Verify Authenticode signature on a DLL file.
/// Returns true if the file is signed by a trusted publisher.
static bool VerifyAuthenticode(const std::wstring& filePath) {
  WINTRUST_FILE_INFO fileInfo{};
  fileInfo.cbStruct = sizeof(fileInfo);
  fileInfo.pcwszFilePath = filePath.c_str();

  WINTRUST_DATA trustData{};
  trustData.cbStruct = sizeof(trustData);
  trustData.dwUIChoice = WTD_UI_NONE;
  trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  trustData.dwUnionChoice = WTD_CHOICE_FILE;
  trustData.pFile = &fileInfo;
  trustData.dwStateAction = WTD_STATEACTION_VERIFY;
  trustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

  GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  LONG status = ::WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &trustData);

  // Clean up trust state.
  trustData.dwStateAction = WTD_STATEACTION_CLOSE;
  ::WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &trustData);

  return status == ERROR_SUCCESS;
}

/// Create a directory with owner + SYSTEM-only ACL (no access for other users).
/// Returns true if the directory exists and has correct permissions.
static bool CreateSecureDirectory(const std::wstring& path) {
  // Build a security descriptor granting full control to the current user
  // and SYSTEM only. This prevents other users from planting DLLs.
  HANDLE token = nullptr;
  PSECURITY_DESCRIPTOR pSD = nullptr;
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = FALSE;

  if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
    DWORD tokenUserSize = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &tokenUserSize);
    if (tokenUserSize > 0) {
      auto buf = std::make_unique<BYTE[]>(tokenUserSize);
      if (::GetTokenInformation(token, TokenUser, buf.get(), tokenUserSize, &tokenUserSize)) {
        LPWSTR sidStr = nullptr;
        auto* tokenUser = reinterpret_cast<TOKEN_USER*>(buf.get());
        if (::ConvertSidToStringSidW(tokenUser->User.Sid, &sidStr)) {
          // SDDL: protected DACL, owner full control + SYSTEM full control.
          std::wstring sddl = L"D:P(A;OICI;FA;;;";
          sddl += sidStr;
          sddl += L")(A;OICI;FA;;;SY)";
          ::ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                                 &pSD, nullptr);
          ::LocalFree(sidStr);
        }
      }
    }
    ::CloseHandle(token);
  }

  if (pSD) sa.lpSecurityDescriptor = pSD;
  BOOL created = ::CreateDirectoryW(path.c_str(), pSD ? &sa : nullptr);
  if (pSD) ::LocalFree(pSD);

  if (!created && ::GetLastError() != ERROR_ALREADY_EXISTS) {
    return false;
  }
  return true;
}

/// Load WindowsPackageManager.dll from the App Installer MSIX package.
/// The WindowsApps directory blocks LoadLibrary, so we copy the DLL to
/// a process-specific secure temp directory and verify its signature.
static bool EnsureInProcModule() {
  if (g_inproc_dll) return true;

  auto pkgPath = FindAppInstallerPath();
  if (pkgPath.empty()) return false;

  // Create a process-specific cache directory to prevent cross-process races.
  wchar_t tempBase[MAX_PATH];
  if (!::GetTempPathW(MAX_PATH, tempBase)) return false;
  std::wstring cacheDir =
      std::wstring(tempBase) + L"winget_nc_" + std::to_wstring(::GetCurrentProcessId());
  if (!CreateSecureDirectory(cacheDir)) return false;

  // Copy WindowsPackageManager.dll to the secure cache.
  std::wstring srcDll = pkgPath + L"\\WindowsPackageManager.dll";
  std::wstring dstDll = cacheDir + L"\\WindowsPackageManager.dll";

  // Always overwrite to prevent stale/tampered files from being used.
  if (!::CopyFileW(srcDll.c_str(), dstDll.c_str(), FALSE)) {
    fprintf(stderr, "winget_nc: failed to copy InProc DLL (error %lu)\n", ::GetLastError());
    return false;
  }

  // Verify Authenticode signature before loading.
  if (!VerifyAuthenticode(dstDll)) {
    fprintf(stderr, "winget_nc: InProc DLL signature verification failed\n");
    ::DeleteFileW(dstDll.c_str());
    return false;
  }

  // Add the App Installer package directory for VC runtime dependency resolution.
  // This avoids copying CRT DLLs to the temp directory.
  g_inproc_dir_cookie = ::AddDllDirectory(pkgPath.c_str());

  g_inproc_dll = ::LoadLibraryExW(
      dstDll.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

  if (!g_inproc_dll) {
    fprintf(stderr, "winget_nc: failed to load InProc DLL: %ls (error %lu)\n", dstDll.c_str(),
            ::GetLastError());
    if (g_inproc_dir_cookie) ::RemoveDllDirectory(g_inproc_dir_cookie);
    g_inproc_dir_cookie = nullptr;
    return false;
  }

  auto initFn = reinterpret_cast<InProcInitialize_t>(
      ::GetProcAddress(g_inproc_dll, "WindowsPackageManagerInProcModuleInitialize"));
  g_inproc_get_factory = reinterpret_cast<InProcGetActivationFactory_t>(
      ::GetProcAddress(g_inproc_dll, "WindowsPackageManagerInProcModuleGetActivationFactory"));
  g_inproc_get_class_object = reinterpret_cast<InProcGetClassObject_t>(
      ::GetProcAddress(g_inproc_dll, "WindowsPackageManagerInProcModuleGetClassObject"));
  g_inproc_terminate = reinterpret_cast<InProcTerminate_t>(
      ::GetProcAddress(g_inproc_dll, "WindowsPackageManagerInProcModuleTerminate"));

  if (!g_inproc_get_factory && !g_inproc_get_class_object) {
    fprintf(stderr, "winget_nc: InProc DLL has no activation exports\n");
    ::FreeLibrary(g_inproc_dll);
    g_inproc_dll = nullptr;
    if (g_inproc_dir_cookie) ::RemoveDllDirectory(g_inproc_dir_cookie);
    g_inproc_dir_cookie = nullptr;
    return false;
  }

  // Initialize the in-process module.
  if (initFn) {
    HRESULT hr = initFn();
    if (FAILED(hr)) {
      fprintf(stderr, "winget_nc: InProcModuleInitialize failed: 0x%08X\n",
              static_cast<unsigned>(hr));
      ::FreeLibrary(g_inproc_dll);
      g_inproc_dll = nullptr;
      g_inproc_get_factory = nullptr;
      g_inproc_get_class_object = nullptr;
      g_inproc_terminate = nullptr;
      if (g_inproc_dir_cookie) ::RemoveDllDirectory(g_inproc_dir_cookie);
      g_inproc_dir_cookie = nullptr;
      return false;
    }
  }

  // Set up the C++/WinRT activation handler so that all WinGet type
  // constructions (FindPackagesOptions, PackageMatchFilter, etc.) go through
  // the InProc module rather than failing with "Class not registered".
  if (g_inproc_get_factory) {
    winrt_activation_handler = [](void* classId, winrt::guid const& iid,
                                  void** factory) noexcept -> int32_t {
      // Check if this is a WinGet type (Microsoft.Management.Deployment.*).
      UINT32 len = 0;
      auto str = ::WindowsGetStringRawBuffer(static_cast<HSTRING>(classId), &len);
      if (str && len > 40 && wcsncmp(str, L"Microsoft.Management.Deployment.", 32) == 0) {
        // Activate through the InProc module.
        winrt::com_ptr<IActivationFactory> af;
        HRESULT hr = g_inproc_get_factory(static_cast<HSTRING>(classId), af.put());
        if (SUCCEEDED(hr) && af) {
          return af->QueryInterface(iid, factory);
        }
        return hr;
      }
      // Not a WinGet type — use standard WinRT activation.
      return WINRT_IMPL_RoGetActivationFactory(classId, iid, factory);
    };
  }

  fprintf(stderr, "winget_nc: loaded InProc module from: %ls\n", dstDll.c_str());
  return true;
}

/// Load Microsoft.Management.Deployment.dll from next to winget_nc.dll.
/// Returns true if already loaded or successfully loaded now.
static bool EnsureInteropDll() {
  if (g_interop_dll) return true;

  // Locate winget_nc.dll's own directory.
  HMODULE self = nullptr;
  if (!::GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(&EnsureInteropDll), &self)) {
    fprintf(stderr, "winget_nc: GetModuleHandleExW failed: %lu\n", ::GetLastError());
    return false;
  }
  wchar_t path[MAX_PATH];
  DWORD len = ::GetModuleFileNameW(self, path, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    fprintf(stderr, "winget_nc: GetModuleFileNameW failed: %lu\n", ::GetLastError());
    return false;
  }

  // Replace "winget_nc.dll" with "Microsoft.Management.Deployment.dll"
  wchar_t* lastSlash = wcsrchr(path, L'\\');
  if (!lastSlash) {
    fprintf(stderr, "winget_nc: no backslash in module path: %ls\n", path);
    return false;
  }
  wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash + 1 - path),
           L"Microsoft.Management.Deployment.dll");

  fprintf(stderr, "winget_nc: loading interop DLL: %ls\n", path);
  g_interop_dll = ::LoadLibraryW(path);
  if (!g_interop_dll) {
    fprintf(stderr, "winget_nc: LoadLibraryW failed: %lu\n", ::GetLastError());
    return false;
  }

  g_get_factory = reinterpret_cast<DllGetActivationFactory_t>(
      ::GetProcAddress(g_interop_dll, "DllGetActivationFactory"));
  g_get_class_object =
      reinterpret_cast<DllGetClassObject_t>(::GetProcAddress(g_interop_dll, "DllGetClassObject"));
  if (!g_get_factory && !g_get_class_object) {
    fprintf(stderr, "winget_nc: no activation exports found\n");
    ::FreeLibrary(g_interop_dll);
    g_interop_dll = nullptr;
    return false;
  }
  return true;
}

/// Activate a WinGet COM class.
/// Tries the in-process module from App Installer first (no package identity
/// needed), then falls back to the NuGet interop DLL and CoCreateInstance.
template <typename T>
T ActivateWinGetClass(std::wstring_view className) {
  // Strategy 1: App Installer in-process module (WindowsPackageManager.dll).
  // This works from unpackaged processes — no package identity required.
  if (EnsureInProcModule() && g_inproc_get_factory) {
    winrt::hstring hsName{className};
    winrt::com_ptr<IActivationFactory> factory;
    HRESULT hr = g_inproc_get_factory(static_cast<HSTRING>(winrt::get_abi(hsName)), factory.put());
    if (SUCCEEDED(hr)) {
      winrt::Windows::Foundation::IInspectable instance;
      hr = factory->ActivateInstance(reinterpret_cast<::IInspectable**>(winrt::put_abi(instance)));
      if (SUCCEEDED(hr)) {
        try {
          return instance.as<T>();
        } catch (const winrt::hresult_error& e) {
          fprintf(stderr, "winget_nc: InProc ActivationFactory QI failed: 0x%08X %ls\n",
                  static_cast<unsigned>(e.code()), e.message().c_str());
        }
      } else {
        fprintf(stderr, "winget_nc: InProc ActivateInstance failed: 0x%08X\n",
                static_cast<unsigned>(hr));
      }
    } else {
      fprintf(stderr, "winget_nc: InProc GetActivationFactory failed: 0x%08X\n",
              static_cast<unsigned>(hr));
    }
  }

  // Also try InProc DllGetClassObject if the activation factory path failed.
  if (g_inproc_dll && g_inproc_get_class_object) {
    static constexpr CLSID CLSID_PM_Standard = {
        0xC53A4F16, 0x787E, 0x42A4, {0xB3, 0x04, 0x29, 0xEF, 0xFB, 0x4B, 0xF5, 0x97}};
    static constexpr CLSID CLSID_PM_Elevated = {
        0x526534B8, 0x7E46, 0x47C8, {0x84, 0x16, 0xB1, 0x68, 0x5C, 0x32, 0x7D, 0x37}};

    const auto& clsid = WgManager::IsElevated() ? CLSID_PM_Elevated : CLSID_PM_Standard;

    winrt::com_ptr<::IClassFactory> factory;
    HRESULT hr = g_inproc_get_class_object(clsid, IID_IClassFactory, factory.put_void());
    if (SUCCEEDED(hr)) {
      winrt::com_ptr<::IUnknown> unk;
      hr = factory->CreateInstance(nullptr, IID_IUnknown, unk.put_void());
      if (SUCCEEDED(hr)) {
        try {
          return unk.as<T>();
        } catch (const winrt::hresult_error& e) {
          fprintf(stderr, "winget_nc: InProc ClassObject QI failed: 0x%08X %ls\n",
                  static_cast<unsigned>(e.code()), e.message().c_str());
        }
      }
    }
  }

  // Strategy 2: NuGet interop DLL's DllGetClassObject.
  if (EnsureInteropDll() && g_get_class_object) {
    static constexpr CLSID CLSID_PM_Standard = {
        0xC53A4F16, 0x787E, 0x42A4, {0xB3, 0x04, 0x29, 0xEF, 0xFB, 0x4B, 0xF5, 0x97}};
    static constexpr CLSID CLSID_PM_Elevated = {
        0x526534B8, 0x7E46, 0x47C8, {0x84, 0x16, 0xB1, 0x68, 0x5C, 0x32, 0x7D, 0x37}};

    const auto& clsid = WgManager::IsElevated() ? CLSID_PM_Elevated : CLSID_PM_Standard;

    winrt::com_ptr<::IClassFactory> factory;
    HRESULT hr = g_get_class_object(clsid, IID_IClassFactory, factory.put_void());
    if (SUCCEEDED(hr)) {
      winrt::com_ptr<::IUnknown> unk;
      hr = factory->CreateInstance(nullptr, IID_IUnknown, unk.put_void());
      if (SUCCEEDED(hr)) {
        try {
          return unk.as<T>();
        } catch (const winrt::hresult_error& e) {
          fprintf(stderr, "winget_nc: DllGetClassObject QI failed: 0x%08X %ls\n",
                  static_cast<unsigned>(e.code()), e.message().c_str());
        }
      } else {
        fprintf(stderr, "winget_nc: IClassFactory::CreateInstance failed: 0x%08X\n",
                static_cast<unsigned>(hr));
      }
    } else {
      fprintf(stderr, "winget_nc: DllGetClassObject failed: 0x%08X\n", static_cast<unsigned>(hr));
    }
  }

  // Strategy 3: WinRT activation via the interop DLL's DllGetActivationFactory.
  if (g_get_factory) {
    winrt::hstring hsName{className};
    winrt::com_ptr<IActivationFactory> factory;
    HRESULT hr = g_get_factory(static_cast<HSTRING>(winrt::get_abi(hsName)), factory.put());
    if (SUCCEEDED(hr)) {
      winrt::Windows::Foundation::IInspectable instance;
      hr = factory->ActivateInstance(reinterpret_cast<::IInspectable**>(winrt::put_abi(instance)));
      if (SUCCEEDED(hr)) {
        try {
          return instance.as<T>();
        } catch (const winrt::hresult_error& e) {
          fprintf(stderr, "winget_nc: DllGetActivationFactory QI failed: 0x%08X %ls\n",
                  static_cast<unsigned>(e.code()), e.message().c_str());
        }
      }
    }
  }

  // Strategy 4: Standard CoCreateInstance (works if caller has package identity).
  {
    static constexpr CLSID CLSID_PM = {
        0xC53A4F16, 0x787E, 0x42A4, {0xB3, 0x04, 0x29, 0xEF, 0xFB, 0x4B, 0xF5, 0x97}};

    winrt::com_ptr<::IUnknown> unk;
    HRESULT hr = ::CoCreateInstance(CLSID_PM, nullptr, CLSCTX_ALL, IID_IUnknown, unk.put_void());
    if (SUCCEEDED(hr)) {
      try {
        return unk.as<T>();
      } catch (const winrt::hresult_error& e) {
        fprintf(stderr, "winget_nc: CoCreateInstance QI failed: 0x%08X %ls\n",
                static_cast<unsigned>(e.code()), e.message().c_str());
      }
    } else {
      fprintf(stderr, "winget_nc: CoCreateInstance failed: 0x%08X\n", static_cast<unsigned>(hr));
    }
  }

  fprintf(stderr, "winget_nc: all activation strategies failed\n");
  winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_PACKAGES_IN_USE));
}

/// Clean up the in-process module on shutdown.
void ShutdownInProcModule() {
  if (g_inproc_terminate) {
    g_inproc_terminate();
    g_inproc_terminate = nullptr;
  }
  g_inproc_get_factory = nullptr;
  g_inproc_get_class_object = nullptr;
  winrt_activation_handler = nullptr;
  if (g_inproc_dll) {
    ::FreeLibrary(g_inproc_dll);
    g_inproc_dll = nullptr;
  }
  if (g_inproc_dir_cookie) {
    ::RemoveDllDirectory(g_inproc_dir_cookie);
    g_inproc_dir_cookie = nullptr;
  }
}

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
  hwnd_ = ::CreateWindowExW(0, L"WingetNcApartment", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                            nullptr, this);

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
    auto* self = reinterpret_cast<WgManager*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
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

WgManager::~WgManager() {
  Stop();
  ShutdownInProcModule();
}

void WgManager::Dispatch(std::function<void()> fn) {
  {
    std::lock_guard lk(task_mutex_);
    task_queue_.push_back(std::move(fn));
  }
  if (hwnd_) ::PostMessageW(hwnd_, WM_DISPATCH_TASK, 0, 0);
}

PackageManager WgManager::CreatePackageManager() {
  return ActivateWinGetClass<PackageManager>(L"Microsoft.Management.Deployment.PackageManager");
}

// Explicit instantiation for cross-TU use.
template PackageManager ActivateWinGetClass<PackageManager>(std::wstring_view);

}  // namespace winget_nc

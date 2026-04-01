// WindowsPackageManager.h
// SPDX-License-Identifier: MIT
//
// Vendored from NuGet package: Microsoft.WindowsPackageManager.ComInterop
// https://www.nuget.org/packages/Microsoft.WindowsPackageManager.ComInterop
//
// To update:
//   1. Download the NuGet package:
//      nuget install Microsoft.WindowsPackageManager.ComInterop -OutputDirectory tmp
//   2. Copy the header:
//      cp tmp/Microsoft.WindowsPackageManager.ComInterop.*/include/WindowsPackageManager.h \
//         native/third_party/winget_interop/include/
//   3. Copy the import libraries:
//      cp tmp/Microsoft.WindowsPackageManager.ComInterop.*/lib/x64/WindowsPackageManager.lib \
//         native/third_party/winget_interop/lib/x64/
//      cp tmp/Microsoft.WindowsPackageManager.ComInterop.*/lib/arm64/WindowsPackageManager.lib \
//         native/third_party/winget_interop/lib/arm64/
//
// This is a placeholder. The real header declares the COM factory classes:
//   - WindowsPackageManagerStandardFactory
//   - WindowsPackageManagerElevatedFactory
// which are used to create IPackageManager instances.

#pragma once

// Placeholder — replace with actual vendored header from NuGet package.
// The real header provides COM activation factories for WinGet:
//
//   class WindowsPackageManagerStandardFactory {
//   public:
//     winrt::Microsoft::Management::Deployment::PackageManager
//     CreatePackageManager();
//   };
//
//   class WindowsPackageManagerElevatedFactory {
//   public:
//     winrt::Microsoft::Management::Deployment::PackageManager
//     CreatePackageManager();
//   };

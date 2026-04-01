// Copyright (c) 2020, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Vendored from the Dart SDK: runtime/include/dart_api_dl.h
//
// To update, copy dart_api_dl.h and dart_api_dl.c from a Dart SDK release:
//   <dart-sdk>/include/dart_api_dl.h
//   <dart-sdk>/include/dart_api_dl.c
//
// Also copy the supporting headers:
//   <dart-sdk>/include/dart_api.h
//   <dart-sdk>/include/dart_native_api.h
//   <dart-sdk>/include/dart_tools_api.h

#ifndef DART_API_DL_H_
#define DART_API_DL_H_

#include "dart_native_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Dart API DL. Must be called with NativeApi.initializeApiDLData
// before any Dart_*_DL function is used.
//
// Returns 0 on success.
DART_EXTERN intptr_t Dart_InitializeApiDL(void* data);

// Posts a C object to the Dart side via a native port.
// This is the dynamically linked version of Dart_PostCObject.
DART_EXTERN bool Dart_PostCObject_DL(Dart_Port port_id, Dart_CObject* message);

// Creates a new native port and returns its id.
DART_EXTERN Dart_Port Dart_NewNativePort_DL(
    const char* name,
    Dart_NativeMessageHandler handler,
    bool handle_concurrently);

// Closes a native port.
DART_EXTERN bool Dart_CloseNativePort_DL(Dart_Port native_port_id);

#ifdef __cplusplus
}
#endif

#endif // DART_API_DL_H_

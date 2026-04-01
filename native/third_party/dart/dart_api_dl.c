// Copyright (c) 2020, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Vendored from the Dart SDK: runtime/include/dart_api_dl.c
//
// Stub implementation for Phase 1 skeleton build verification.
// Replace with the actual dart_api_dl.c from the Dart SDK for production.
//
// To obtain the real file:
//   1. Download a Dart SDK release
//   2. Copy runtime/include/dart_api_dl.c here
//   OR
//   3. Use: dart pub cache list  →  find the SDK path  →  copy from include/

#include "dart_api_dl.h"

#include <stddef.h>
#include <string.h>

// Function pointers resolved by Dart_InitializeApiDL.
static bool (*Dart_PostCObject_)(Dart_Port port_id, Dart_CObject* message) = NULL;
static Dart_Port (*Dart_NewNativePort_)(const char* name,
                                        Dart_NativeMessageHandler handler,
                                        bool handle_concurrently) = NULL;
static bool (*Dart_CloseNativePort_)(Dart_Port native_port_id) = NULL;

// Stub: in production, this parses the DL data struct from the Dart VM
// and populates the function pointers above.
intptr_t Dart_InitializeApiDL(void* data) {
  // TODO: Replace with real Dart SDK implementation that resolves
  // function pointers from the NativeApi data structure.
  (void)data;
  return 0;
}

bool Dart_PostCObject_DL(Dart_Port port_id, Dart_CObject* message) {
  if (Dart_PostCObject_ == NULL) return false;
  return Dart_PostCObject_(port_id, message);
}

Dart_Port Dart_NewNativePort_DL(const char* name,
                                Dart_NativeMessageHandler handler,
                                bool handle_concurrently) {
  if (Dart_NewNativePort_ == NULL) return ILLEGAL_PORT;
  return Dart_NewNativePort_(name, handler, handle_concurrently);
}

bool Dart_CloseNativePort_DL(Dart_Port native_port_id) {
  if (Dart_CloseNativePort_ == NULL) return false;
  return Dart_CloseNativePort_(native_port_id);
}

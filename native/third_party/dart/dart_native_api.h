// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Vendored from the Dart SDK: runtime/include/dart_native_api.h
//
// Minimal subset needed for winget_nc bridge compilation.
// Replace with full Dart SDK headers for production builds.

#ifndef DART_NATIVE_API_H_
#define DART_NATIVE_API_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DART_EXTERN
#ifdef _WIN32
#define DART_EXTERN extern
#else
#define DART_EXTERN extern __attribute__((visibility("default")))
#endif
#endif

// Dart_Port is an int64_t used to identify a native port.
typedef int64_t Dart_Port;

#define ILLEGAL_PORT ((Dart_Port)0)

// Dart_CObject types used by the native messaging API.
typedef enum {
  Dart_CObject_kNull = 0,
  Dart_CObject_kBool,
  Dart_CObject_kInt32,
  Dart_CObject_kInt64,
  Dart_CObject_kDouble,
  Dart_CObject_kString,
  Dart_CObject_kArray,
  Dart_CObject_kTypedData,
  Dart_CObject_kExternalTypedData,
  Dart_CObject_kSendPort,
  Dart_CObject_kCapability,
  Dart_CObject_kNativePointer,
  Dart_CObject_kUnsupported,
  Dart_CObject_kNumberOfTypes,
} Dart_CObject_Type;

typedef struct _Dart_CObject {
  Dart_CObject_Type type;
  union {
    bool as_bool;
    int32_t as_int32;
    int64_t as_int64;
    double as_double;
    char* as_string;
    struct {
      intptr_t length;
      struct _Dart_CObject** values;
    } as_array;
    // Additional union members omitted for brevity.
    // See full Dart SDK dart_native_api.h for complete definition.
  } value;
} Dart_CObject;

// Handler for native messages.
typedef void (*Dart_NativeMessageHandler)(Dart_Port dest_port_id,
                                          Dart_CObject* message);

#ifdef __cplusplus
}
#endif

#endif // DART_NATIVE_API_H_

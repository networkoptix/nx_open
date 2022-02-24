// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef NX_SOCKET_SEQUENCE_H
#define NX_SOCKET_SEQUENCE_H

#include <stdint.h>


#ifdef __arm__
// Some devices require kernel update to support 64-bit atomics.
typedef int SocketSequenceType;
#else
typedef uint64_t SocketSequenceType;
#endif

#endif  //NX_SOCKET_SEQUENCE_H

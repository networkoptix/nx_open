// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scope_guard.h>
#include <nx/utils/uuid.h>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAuthenticator>

/**
 * Convenience macro for generating stream operators of enumerations.
 */
#define QN_DEFINE_ENUM_STREAM_OPERATORS(ENUM)                                   \
inline QDataStream &operator<<(QDataStream &stream, const ENUM &value) {        \
    return stream << static_cast<int>(value);                                   \
}                                                                               \
inline QDataStream &operator>>(QDataStream &stream, ENUM &value) {              \
    int tmp;                                                                    \
    stream >> tmp;                                                              \
    value = static_cast<ENUM>(tmp);                                             \
    return stream;                                                              \
}

/**
 * Convenience class for uniform initialization of metatypes in common module.
 */
class NX_VMS_COMMON_API QnCommonMetaTypes
{
public:
    static void initialize();
};

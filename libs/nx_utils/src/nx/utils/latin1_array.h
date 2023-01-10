// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_LATIN1_ARRAY_H
#define QN_LATIN1_ARRAY_H

#include <QtCore/QByteArray>

#include <nx/reflect/from_string.h>
#include <nx/reflect/to_string.h>

/**
 * A byte array that is to be treated by serialization / deserialization
 * routines as a latin1 string.
 */
class QnLatin1Array: public QByteArray {
public:
    QnLatin1Array() {}

    QnLatin1Array(const QByteArray &other): QByteArray(other) {}
    QnLatin1Array(const char* data): QByteArray(data) {}
};

namespace nx::reflect::detail {

template<> class HasToBase64<QnLatin1Array>: public std::false_type {};
template<> struct IsQByteArrayAlike<QnLatin1Array, std::void_t<>>: public std::false_type {};

} // namespace nx::reflect::detail


#endif // QN_LATIN1_ARRAY_H

#pragma once

#include <stdint.h>
#include <string.h> // CBC mode, for memset

#include <QtCore>

namespace nx {
namespace utils {
QByteArray encodeSimple(const QByteArray& data);

// NOTE: data to be encoded shouldn't contain '\0' because decrypted 
//       data is stripped at the first '\0' symbol
QByteArray encodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key);
QByteArray decodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key);

// Convenient overloads. If key is less than 16 bytes, 
// it is completed with 0x01, 0x02, ... sequence.
// Otherwise it is stripped off to 16 bytes.
QByteArray encodeAES128CBC(const QByteArray& data, const QByteArray& key);
QByteArray decodeAES128CBC(const QByteArray& data, const QByteArray& key);

// These overloads use internal constant key
QByteArray encodeAES128CBC(const QByteArray& data);
QByteArray decodeAES128CBC(const QByteArray& data);
}
}
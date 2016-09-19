#pragma once

#include <stdint.h>
#include <string.h> // CBC mode, for memset

#include <QtCore>

namespace nx {
namespace utils {
QByteArray encodeSimple(const QByteArray& data);
QByteArray encodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key);
QByteArray decodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key)
}
}
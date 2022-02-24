// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <string>

#include <QtCore/QByteArray>
#include <QtCore/QLatin1String>

namespace nx {
namespace utils {

enum Crc16Type
{
    ibmReversed = 0,
};

NX_UTILS_API std::uint16_t crc16(const char* data, std::size_t size, Crc16Type type = Crc16Type::ibmReversed);
NX_UTILS_API std::uint16_t crc16(std::string_view str, Crc16Type type = Crc16Type::ibmReversed);
NX_UTILS_API std::uint16_t crc16(const QByteArray& data, Crc16Type type = Crc16Type::ibmReversed);
NX_UTILS_API std::uint16_t crc16(const QLatin1String& str, Crc16Type type = Crc16Type::ibmReversed);

} // namespace utils
} // namespace nx

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "crc16.h"
#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {

namespace {

static uint16_t crc16ibmRev(const uint8_t* ptr, size_t count)
{
   uint16_t crc = 0;
   while (count--)
   {
      crc ^= (uint16_t) *ptr++;
      for (int i = 0; i < 8; ++i)
      {
         if ((crc & 0x01) != 0)
            crc = (crc >> 1) ^ 0xA001;
         else
            crc >>= 1;
      }
   }
   return crc;
}

} // namespace

uint16_t crc16(const char* data, std::size_t size, Crc16Type type)
{
    switch (type)
    {
        case Crc16Type::ibmReversed:
            return crc16ibmRev((const uint8_t*) data, size);
        default:
            NX_ASSERT(false); //< Not implemented.
            return 0;
    }
}

uint16_t crc16(std::string_view str, Crc16Type type)
{
    return crc16(str.data(), str.size(), type);
}

uint16_t crc16(const QByteArray& data, Crc16Type type)
{
    return crc16(data.data(), data.size(), type);
}

uint16_t crc16(const QLatin1String& str, Crc16Type type)
{
    return crc16(str.data(), str.size(), type);
}

} // namespace utils
} // namespace nx

#pragma once

#include <string>
#include <stdint.h>

#include <limits>

#include <QtCore/QByteArray>
#include <boost/functional/hash/hash.hpp>

#ifdef _WIN32
#undef max
#undef min
#endif

namespace nx {

/**
 * Some effective buffer is required. Following is desired:\n
 * - substr O(1) complexity
 * - pop_front, pop_back O(1) complexity
 * - concatenation O(1) complexity. This implies readiness for readv and writev system calls
 * - buffer should implicit sharing but still minimize atomic operations where they not required
 * Currently, using QByteArray, but fully new implementation will be provided one day...
 */
typedef QByteArray Buffer;

/**
 * Some effective buffer is required. Following is desired:
 * - all features of Buffer
 * - QString compartability (with checkups for non ASCII symbols)
 */
typedef QByteArray String;

bool NX_NETWORK_API operator==(const std::string& left, const nx::String& right);
bool NX_NETWORK_API operator==(const nx::String& left, const std::string& right);
bool NX_NETWORK_API operator!=(const std::string& left, const nx::String& right);
bool NX_NETWORK_API operator!=(const nx::String& left, const std::string& right);

String NX_NETWORK_API operator+(const String& left, const std::string& right);
String NX_NETWORK_API operator+(const String& left, const QString& right);

/** Converts zero terminated string into buffer including single 0 byte. */
Buffer NX_NETWORK_API stringToBuffer(const String& string);

/** Extracts zero terminated string from Buffer without trailing 0 bytes if any. */
String NX_NETWORK_API bufferToString(const Buffer& buffer);

std::string NX_NETWORK_API toStdString(const String& str);

template<typename Arg>
void replace(QByteArray* where, int pos, int count, const Arg& withWhat)
{
    where->replace(pos, count, withWhat);
}

template<>
inline void replace(QByteArray* where, int pos, int count, const std::string& withWhat)
{
    where->replace(pos, count, withWhat.c_str());
}

} // namespace nx

namespace std {

template<>
struct hash< ::nx::Buffer >
{
    size_t operator()(const ::nx::Buffer& buffer) const
    {
        return boost::hash_range(buffer.begin(), buffer.end());
    }
};

} // namespace std

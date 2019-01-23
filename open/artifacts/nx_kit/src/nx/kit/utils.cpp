// Copyright 2018-present Network Optix, Inc.
#include "utils.h"

#include <cstdarg>

namespace nx {
namespace kit {
namespace utils {

std::string format(const std::string formatStr, ...)
{
    va_list args;
    va_start(args, formatStr);
    const int size = vsnprintf(nullptr, 0, formatStr.c_str(), args) + /* Space for '\0' */ 1;
    va_end(args);

    if (size < 0)
        return formatStr; //< No better way to handle out-of-memory-like errors.

    std::unique_ptr<char[]> buf(new char[size]);

    va_start(args, formatStr);
    vsnprintf(buf.get(), (size_t) size, formatStr.c_str(), args);
    va_end(args);

    return std::string(buf.get(), buf.get() + size - /* Trim trailing '\0' */ 1);
}

bool isAsciiPrintable(int c)
{
    return c >= 32 && c <= 126;
}

std::string toString(std::string s)
{
    return toString(s.c_str());
}

std::string toString(uint8_t i)
{
    return toString((int) i);
}

std::string toString(char c)
{
    if (!isAsciiPrintable(c))
        return format("'\\x%02X'", (unsigned char) c);
    if (c == '\'')
        return "'\\''";
    return std::string("'") + c + "'";
}

std::string toString(const char* s)
{
    if (s == nullptr)
        return "null";

    std::string str;
    for (const char* p = s; *p != '\0'; ++p)
    {
        if (*p == '\\' || *p == '"')
            (str += '\\') += *p;
        else if (*p == '\n')
            str += "\\n";
        else if (*p == '\t')
            str += "\\t";
        else if (!isAsciiPrintable(*p))
            str.append(format("\\x%02X", (unsigned char) *p));
        else
            str += *p;
    }
    return "\"" + str + "\"";
}

std::string toString(char* s)
{
    return toString(const_cast<const char*>(s));
}

std::string toString(const void* ptr)
{
    return ptr ? format("%p", ptr) : "null";
}

std::string toString(void* ptr)
{
    return toString(const_cast<const void*>(ptr));
}

std::string toString(std::nullptr_t ptr)
{
    return toString((const void*) ptr);
}

std::string toString(bool b)
{
    return b ? "true" : "false";
}

uint8_t* unalignedPtr(void* data)
{
    static const int kMediaAlignment = 32;
    return (uint8_t*) (
        17 + (((uintptr_t) data) + kMediaAlignment - 1) / kMediaAlignment * kMediaAlignment);
}

} // namespace utils
} // namespace kit
} // namespace nx

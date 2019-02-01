// Copyright 2018-present Network Optix, Inc.
#include "utils.h"

namespace nx {
namespace kit {
namespace utils {

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
    std::string result;
    if (s == nullptr)
    {
        result = "null";
    }
    else
    {
        result = "\"";
        for (const char* p = s; *p != '\0'; ++p)
        {
            if (*p == '\\' || *p == '"')
                (result += '\\') += *p;
            else if (*p == '\n')
                result += "\\n";
            else if (*p == '\t')
                result += "\\t";
            else if (!isAsciiPrintable(*p))
                result += format("\\x%02X", (unsigned char) *p);
            else
                result += *p;
        }
        result += "\"";
    }
    return result;
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

} // namespace utils
} // namespace kit
} // namespace nx

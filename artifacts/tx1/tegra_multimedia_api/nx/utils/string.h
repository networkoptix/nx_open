#pragma once

#include <string>
#include <cstdio>
#include <cstdarg>

namespace nx {
namespace kit {
namespace debug {

// TODO: #mshevchenko
#if 0 // Variadic template impl.

template <typename... Args>
std::string stringFormat(const std::string& format, Args... args)
{
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'.
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside.
}

#else // Variadic function impl.

static inline std::string format(const std::string formatStr, ...)
{
    va_list args;
    va_start(args, formatStr);
    int size = vsnprintf(nullptr, 0, formatStr.c_str(), args) + 1; //< Extra space for '\0'.
    if (size < 0)
    {
        va_end(args);
        return formatStr; //< No better way to handle out-of-memory-like errors.
    }
    std::unique_ptr<char[]> buf(new char[size]);
    va_end(args);

    va_start(args, formatStr);
    vsnprintf(buf.get(), (size_t) size, formatStr.c_str(), args);
    va_end(args);
    return std::string(buf.get(), buf.get() + size - 1); //< Trim trailing '\0'.
}

#endif // 0

} // namespace debug
} // namespace kit
} // namespace nx

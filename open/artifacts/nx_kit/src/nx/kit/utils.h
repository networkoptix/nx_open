// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Various utilities. Used by other nx_kit components.
 *
 * This unit can be compiled in the context of any C++ project. If Qt headers are included before
 * this one, some Qt support is enabled via "#if defined(QT_CORE_LIB)".
 */

#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

#if defined(QT_CORE_LIB)
    // To be supported in toString().
    #include <QtCore/QByteArray>
    #include <QtCore/QString>
    #include <QtCore/QUrl>
#endif

#if !defined(NX_KIT_API)
    #define NX_KIT_API /*empty*/
#endif

namespace nx {
namespace kit {
namespace utils {

//-------------------------------------------------------------------------------------------------
// Strings.

inline bool isAsciiPrintable(int c)
{
    return c >= 32 && c <= 126;
}

/**
 * @return Last path component: text after the last path separator. On Windows, possible `<drive>:`
 * prefix is excluded and both `/` and `\` are supported. If path is empty, the result is empty.
 */
NX_KIT_API std::string baseName(std::string path);

/**
 * @return Process name, without .exe in Windows.
 */
NX_KIT_API std::string getProcessName();

/**
 * Convert a value to its report-friendly text representation, e.g. a quoted and escaped string.
 * Non-printable chars in a string are represented as hex escape sequences like `\xFF""` - note
 * that the two quotes after it are inserted to indicate the end of the hex number, as in C/C++.
 */
template<typename T>
std::string toString(T value);

template<typename... Args>
std::string format(const std::string& formatStr, Args... args)
{
    const size_t size = snprintf(nullptr, 0, formatStr.c_str(), args...) + /* space for '\0' */ 1;
    if (size < 0)
        return formatStr; //< No better way to handle out-of-memory-like errors.
    std::string result(size, '\0');
    snprintf(&result[0], size, formatStr.c_str(), args...);
    result.resize(size - /* trailing '\0' */ 1);
    return result;
}

NX_KIT_API bool fromString(const std::string& s, int* value);
NX_KIT_API bool fromString(const std::string& s, double* value);
NX_KIT_API bool fromString(const std::string& s, float* value);

//-------------------------------------------------------------------------------------------------
// OS support.

/**
 * @return Command line arguments of the process, cached after the first call. If arguments are
 *     not available, then returns a single empty string.
 */
NX_KIT_API const std::vector<std::string>& getProcessCmdLineArgs();

//-------------------------------------------------------------------------------------------------
// Aligned allocation.

/**
 * Aligns value up to alignment boundary.
 * @param alignment If zero, value is returned unchanged.
 */
inline size_t alignUp(size_t value, size_t alignment)
{
    if (alignment == 0)
        return value;
    const size_t remainder = value % alignment;
    if (remainder == 0)
        return value;
    return value + alignment - remainder;
}

/** Shifts the pointer up to deliberately misalign it to an odd address - intended for tests. */
inline uint8_t* misalignedPtr(void* data)
{
    return (uint8_t*) (17 + alignUp((uintptr_t) data, 32));
}

/**
 * Allocates size bytes of data, aligned to alignment boundary.
 *
 * NOTE: Allocated memory must be freed with a call to freeAligned().
 * NOTE: This function is as safe as malloc().
 *
 * @param mallocFunc Function with the signature void*(size_t), which is called to allocate memory.
 */
template<class MallocFunc>
void* mallocAligned(size_t size, size_t alignment, MallocFunc mallocFunc)
{
    if (alignment == 0)
        return 0;
    void* ptr = mallocFunc(size + alignment + sizeof(alignment));
    if (!ptr) //< allocation error
        return ptr;

    void* aligned_ptr = (char*) ptr + sizeof(alignment); //< Leaving place to save misalignment.
    const size_t misalignment = alignment - (((uintptr_t) aligned_ptr) % alignment);
    memcpy((char*) ptr + misalignment, &misalignment, sizeof(misalignment)); //< Save misalignment.
    return (char*) aligned_ptr + misalignment;
}

/** Calls mallocAligned() passing standard malloc() as mallocFunc. */
inline void* mallocAligned(size_t size, size_t alignment)
{
    // NOTE: Lambda is used to suppress a warning that some ::malloc() implementations are using
    // deprecated exception specification.
    return mallocAligned<>(size, alignment, [](size_t size) { return ::malloc(size); });
}

/**
 * Free ptr allocated with a call to mallocAligned().
 *
 * NOTE: This function is as safe as ::free().
 *
 * @param freeFunc Function with the signature void(void*), which is called to free the memory.
 */
template<class FreeFunc>
void freeAligned(void* ptr, FreeFunc freeFunc)
{
    if (!ptr)
        return freeFunc(ptr);

    ptr = (char*) ptr - sizeof(size_t);
    size_t misalignment = 0;
    memcpy(&misalignment, ptr, sizeof(misalignment)); //< Retrieve saved misalignment.
    ptr = (char*) ptr - misalignment;

    freeFunc(ptr);
}

/** Calls freeAligned() passing standard free() as freeFunc. */
inline void freeAligned(void* ptr)
{
    // NOTE: Lambda is used to suppress a warning that some ::free() implementations are using
    // deprecated exception specification.
    return freeAligned<>(ptr, [](void* ptr) { return ::free(ptr); });
}

//-------------------------------------------------------------------------------------------------
// Implementation.

// The order of overloads below is important - it defines which will be chosen by inline functions.
NX_KIT_API std::string toString(bool b);
NX_KIT_API std::string toString(const void* ptr);
inline std::string toString(void* ptr) { return toString(const_cast<const void*>(ptr)); }
inline std::string toString(std::nullptr_t ptr) { return toString((const void*) ptr); }
inline std::string toString(uint8_t i) { return toString((int) i); } //< Avoid matching as char.
inline std::string toString(int8_t i) { return toString((int) i); } //< Avoid matching as char.
NX_KIT_API std::string toString(char c);
NX_KIT_API std::string toString(const char* s);
inline std::string toString(char* s) { return toString(const_cast<const char*>(s)); }
inline std::string toString(const std::string& s) { return toString(s.c_str()); }
NX_KIT_API std::string toString(wchar_t c);
NX_KIT_API std::string toString(const wchar_t* w);
inline std::string toString(wchar_t* w) { return toString(const_cast<const wchar_t*>(w)); }
inline std::string toString(const std::wstring& w) { return toString(w.c_str()); }

// For unknown types, use their operator<<().
template<typename T>
std::string toString(T value)
{
    std::ostringstream outputString;
    outputString << value;
    return outputString.str();
}

#if defined(QT_CORE_LIB)

static inline std::string toString(QByteArray b) //< By value to avoid calling the template impl.
{
    return toString(b.toStdString());
}

static inline std::string toString(QString s) //< By value to avoid calling the template impl.
{
    return toString(s.toUtf8().constData());
}

static inline std::string toString(QUrl u) //< By value to avoid calling the template impl.
{
    return toString(u.toEncoded().toStdString());
}

#endif // defined(QT_CORE_LIB)

template<typename P>
std::string toString(P* ptr)
{
    return toString((const void*) ptr);
}

} // namespace utils
} // namespace kit
} // namespace nx

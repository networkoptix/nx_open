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
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>

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

inline bool isSpaceOrControlChar(char c)
{
    // NOTE: Chars 128..255 should be treated as non-whitespace, thus, isprint() will not do.
    return (((unsigned char) c) <= 32) || (c == 127);
}

/**
 * Decodes a string encoded using C/C++ string literal rules: enquoted, potentially containing
 * escape sequences. Supports concatenation of consecutive literals, thus, fully compatible with
 * strings encoded by nx::kit::utils::toString().
 *
 * @param outErrorMessage In case of any error in the encoded string, the function attempts to
 *     recover using the most obvious way, still producing the result, and reports all such cases
 *     via this argument if it is not null.
 */
NX_KIT_API std::string decodeEscapedString(
    const std::string& s, std::string* outErrorMessage = nullptr);

/**
 * Converts a value to its report-friendly text representation; for strings it being a quoted and
 * C-style-escaped string. Non-printable chars in a string are represented as hex escape sequences
 * like `\xFF""` - note that the two quotes after it are inserted to indicate the end of the hex
 * number, because according to the C/C++ standards, `\x` consumes as much hex digits as possible.
 */
template<typename T>
std::string toString(T value);

/**
 * ATTENTION: std::string is not supported as one of `args`, and will cause undefined behavior.
 */
template<typename... Args>
std::string format(const std::string& formatStr, Args... args)
{
    const int size = snprintf(nullptr, 0, formatStr.c_str(), args...) + /*space for \0*/ 1;
    if (size <= 0)
        return formatStr; //< No better way to handle out-of-memory-like errors.
    std::string result(size, '\0');
    snprintf(&result[0], size, formatStr.c_str(), args...);
    result.resize(size - /*terminating \0*/ 1);
    return result;
}

NX_KIT_API bool fromString(const std::string& s, int* value);
NX_KIT_API bool fromString(const std::string& s, double* value);
NX_KIT_API bool fromString(const std::string& s, float* value);
NX_KIT_API bool fromString(const std::string& s, bool* value);

NX_KIT_API void stringReplaceAllChars(std::string* s, char sample, char replacement);
NX_KIT_API void stringInsertAfterEach(std::string* s, char sample, const char* insertion);
NX_KIT_API void stringReplaceAll(
    std::string* s, const std::string& sample, const std::string& replacement);

// TODO: Remove when migrating to C++20 - it has std::string::starts_with()/ends_with().
NX_KIT_API bool stringStartsWith(const std::string& s, const std::string& prefix);
NX_KIT_API bool stringEndsWith(const std::string& s, const std::string& suffix);

NX_KIT_API std::string trimString(const std::string& s);

//-------------------------------------------------------------------------------------------------
// OS support.

constexpr char kPathSeparator =
    #if defined(_WIN32)
        '\\';
    #else
        '/';
    #endif

/**
 * @return Last path component: text after the last path separator. On Windows, possible `<drive>:`
 * prefix is excluded and both `/` and `\` are supported. If path is empty, the result is empty.
 */
NX_KIT_API std::string baseName(std::string path);

/**
 * If the specified path is absolute, just return it, otherwise, convert it to an absolute path
 * using the specified origin dir. On Windows, both `/` and `\` are supported, and paths without
 * a drive letter but started with `/` or `\` are treated as absolute.
 */
NX_KIT_API std::string absolutePath(
    const std::string& originDir, const std::string& path);

/**
 * @return Process name, without .exe in Windows.
 */
NX_KIT_API std::string getProcessName();

/**
 * @return Command line arguments of the process, cached after the first call. If arguments are
 *     not available, then returns a single empty string.
 */
NX_KIT_API const std::vector<std::string>& getProcessCmdLineArgs();

NX_KIT_API bool fileExists(const char* filename);

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
        return nullptr;
    const auto ptr = (char*) mallocFunc(size + alignment + sizeof(alignment));
    if (!ptr) //< allocation error
        return ptr;

    char* const alignedPtr = ptr + sizeof(alignment); //< Leaving place to save misalignment.
    const size_t misalignment = alignment - (uintptr_t) alignedPtr % alignment;
    memcpy(ptr + misalignment, &misalignment, sizeof(misalignment)); //< Save misalignment.
    return alignedPtr + misalignment;
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
NX_KIT_API std::string toString(wchar_t c);
NX_KIT_API std::string toString(const wchar_t* w);
inline std::string toString(wchar_t* w) { return toString(const_cast<const wchar_t*>(w)); }

// std::string can contain '\0' inside, hence a dedicated implementation.
NX_KIT_API std::string toString(const std::string& s);
NX_KIT_API std::string toString(const std::wstring& w);


/** For unknown types, use their operator<<(). */
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

//-------------------------------------------------------------------------------------------------
// Configuration file parsing.

NX_KIT_API bool parseNameValueFile(
    const std::string& nameValueFilePath,
    std::map<std::string, std::string>* nameValueMap,
    const std::string& errorPrefix,
    std::ostream* out,
    bool* isFileEmpty);

/** Converts ASCII characters from the input string to the upper case. */
NX_KIT_API std::string toUpper(const std::string& str);

} // namespace utils
} // namespace kit
} // namespace nx

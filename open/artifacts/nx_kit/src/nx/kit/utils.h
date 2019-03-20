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
 * Convert a value to its report-friendly text representation, e.g. a quoted and escaped string.
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

//-------------------------------------------------------------------------------------------------
// OS support.

/**
 * @return Command line arguments of the proccess, cached after the first call. If arguments are
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
    return mallocAligned<>(size, alignment, ::malloc);
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
    return freeAligned<>(ptr, ::free);
}

//-------------------------------------------------------------------------------------------------
// Implementation.

template<typename T>
std::string toString(T value)
{
    std::ostringstream outputString;
    outputString << value;
    return outputString.str();
}

NX_KIT_API std::string toString(std::string s); //< By value to avoid calling the template impl.
NX_KIT_API std::string toString(uint8_t i); //< Otherwise, uint8_t would be printed as char.
NX_KIT_API std::string toString(char c);
NX_KIT_API std::string toString(const char* s);
NX_KIT_API std::string toString(char* s);
NX_KIT_API std::string toString(const void* ptr);
NX_KIT_API std::string toString(void* ptr);
NX_KIT_API std::string toString(std::nullptr_t ptr);
NX_KIT_API std::string toString(bool b);

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

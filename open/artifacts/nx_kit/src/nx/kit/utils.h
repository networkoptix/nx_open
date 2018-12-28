// Copyright 2018-present Network Optix, Inc.
#pragma once

/**@file
 * Variuos utilities. Used by other nx_kit components.
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

NX_KIT_API bool isAsciiPrintable(int c);

/**
 * Convert various values to their accurate text representation, e.g. quoted and escaped strings.
 */
template<typename T>
std::string toString(T value);

NX_KIT_API std::string format(std::string formatStr, ...);

//-------------------------------------------------------------------------------------------------
// Aligned allocation.

NX_KIT_API uint8_t* unalignedPtr(void* data);

/** Alignes val up to alignment boundary. */
inline size_t alignUp(size_t val, size_t alignment)
{
    const size_t remainder = val % alignment;
    if (remainder == 0)
        return val;
    return val + (alignment - remainder);
}

/**
 * Allocate size bytes of data, aligned to alignment boundary.
 *
 * NOTE: Allocated memory must be freed with a call to freeAligned().
 *
 * NOTE: This function is as safe as ::malloc().
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

    void* aligned_ptr = (char*) ptr + sizeof(alignment); //< Leaving place to save unalignment.
    const size_t unalignment = alignment - (((size_t) aligned_ptr) % alignment);
    memcpy((char*) ptr+unalignment, &unalignment, sizeof(unalignment));
    return (char*) aligned_ptr + unalignment;
}

/** Calls mallocAligned() passing ::malloc as mallocFunc. */
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
    size_t unalignment = 0;
    memcpy(&unalignment, ptr, sizeof(unalignment));
    ptr = (char*) ptr - unalignment;

    freeFunc(ptr);
}

/** Calls freeAligned() passing ::free as freeFunc. */
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

static inline std::string toString(const QByteArray& b)
{
    return toString(b.toStdString());
}

static inline std::string toString(const QString& s)
{
    return toString(s.toUtf8().constData());
}

static inline std::string toString(const QUrl& u)
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

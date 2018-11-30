#pragma once

/**@file
 * Various tools for plugins. Header-only.
 */

#if defined(_WIN32)
    #include <Windows.h>
    #undef min
    #undef max
#endif

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <iomanip>
#include <limits>
#include <type_traits>

#include "plugin_api.h"

// TODO: #mshevchenko: Split into GUID tools (e.g. nx::sdk::common::Guid) and ref-counting class
// (e.g. nx::sdk::common::Object), merging nx/vms/server/sdk_support/utils.h with the latter.
namespace nxpt {

/**
 * Automatic scoped pointer class which uses PluginInterface reference counting interface
 * (PluginInterface::addRef() and PluginInterface::releaseRef()) instead of new/delete.
 * Increments object's reference counter (PluginInterface::addRef()) at construction, decrements at
 * destruction (PluginInterface::releaseRef()).
 *
 * NOTE: The class is reentrant, but not thread-safe.
 *
 * @param T Must inherit PluginInterface.
 */
template<class T>
class ScopedRef
{
public:
    /** Intended to be applied to queryInterface(). */
    explicit ScopedRef(void* ptr): ScopedRef(static_cast<T*>(ptr), /*increaseRef*/ false) {}

    /** Calls ptr->addRef() if ptr is not null and increaseRef is true. */
    explicit ScopedRef(T* ptr = nullptr, bool increaseRef = true):
        m_ptr(ptr)
    {
        // TODO: #dmishin get rid of this const mess when releaseRef becomes const
        if (m_ptr && increaseRef)
            const_cast<std::remove_const_t<T>*>(m_ptr)->addRef();
    }

    ScopedRef(ScopedRef<T>&& right)
    {
        m_ptr = right.release();
    }

    ScopedRef<T>& operator=(ScopedRef<T>&& right)
    {
        m_ptr = right.release();
        return *this;
    }

    ScopedRef(const ScopedRef<T>&) = delete;

    ScopedRef<T>& operator=(const ScopedRef<T>&) = delete;

    ~ScopedRef() { reset(); }

    operator bool() const { return m_ptr != nullptr; }

    /** @return Protected pointer, without releasing it. */
    T* get() { return m_ptr; }

    /** @return Protected pointer, without releasing it. */
    const T* get() const { return m_ptr; }

    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }

    /** Releases protected pointer without calling nxpl::PluginInterface::releaseRef(). */
    T* release()
    {
        T* ptrBak = m_ptr;
        m_ptr = 0;
        return ptrBak;
    }

    /**
     * Calls nxpl::PluginInterface::releaseRef() on protected pointer (if any) and takes the new
     * pointer ptr (calling nxpl::PluginInterface::addRef()).
     */
    void reset(T* ptr = nullptr)
    {
        if (m_ptr)
        {
            // TODO: #dmishin get rid of this const mess when releaseRef becomes const
            const_cast<std::remove_const_t<T>*>(m_ptr)->releaseRef();
            m_ptr = nullptr;
        }

        take(ptr);
    }

private:
    T* m_ptr;

    void take(T* ptr)
    {
        m_ptr = ptr;
        if(m_ptr)
        {
            // TODO: #dmishin get rid of this const mess when releaseRef becomes const
            const_cast<std::remove_const_t<T>*>(m_ptr)->addRef();
        }
    }
};

namespace atomic {

#ifdef _WIN32
    typedef volatile LONG AtomicLong;
#elif __GNUC__
    typedef volatile long AtomicLong;
#else
    #error "Unsupported compiler is used."
#endif

/** Increments *val, returns the new (incremented) value. */
static AtomicLong inc(AtomicLong* val)
{
    #ifdef _WIN32
        return InterlockedIncrement(val);
    #elif __GNUC__
        return __sync_add_and_fetch(val, 1);
    #endif
}

/** Decrements *val, returns the new (decremented) value. */
static AtomicLong dec(AtomicLong* val)
{
    #ifdef _WIN32
        return InterlockedDecrement(val);
    #elif __GNUC__
        return __sync_sub_and_fetch(val, 1);
    #endif
}

} // namespace atomic

/**
 * Implements nxpl::PluginInterface reference counting. Can delegate reference counting to another
 * CommonRefManager instance.
 *
 * This class does not inherit nxpl::PluginInterface because it would require virtual inheritance.
 * This class instance is supposed to be nested into a monitored class.
 */
class CommonRefManager
{
public:
    CommonRefManager(const CommonRefManager&) = delete;
    CommonRefManager& operator=(const CommonRefManager&) = delete;

    /**
     * Use this constructor to delete objToWatch when the reference counter drops to zero.
     * NOTE: After creation, the reference counter is 1.
     */
    CommonRefManager(nxpl::PluginInterface* objToWatch):
        m_refCount(1),
        m_objToWatch(objToWatch),
        m_refCountingDelegate(0)
    {
    }

    /**
     * Use this constructor to delegate reference counting to another object.
     * NOTE: It does not increment refCountingDelegate reference counter.
     */
    CommonRefManager(CommonRefManager* refCountingDelegate):
        m_refCountingDelegate(refCountingDelegate)
    {
    }

    /** Implementaion of nxpl::PluginInterface::addRef(). */
    unsigned int addRef()
    {
        return m_refCountingDelegate
            ? m_refCountingDelegate->addRef()
            : atomic::inc(&m_refCount);
    }

    /**
     * Implementaion of nxpl::PluginInterface::releaseRef(). Deletes the monitored object if the
     * reference counter drops to zero.
     */
    unsigned int releaseRef()
    {
        if (m_refCountingDelegate)
            return m_refCountingDelegate->releaseRef();

        unsigned int newRefCounter = atomic::dec(&m_refCount);
        if (newRefCounter == 0)
            delete m_objToWatch;
        return newRefCounter;
    }

	unsigned int refCount() const
	{
		if (m_refCountingDelegate)
			return m_refCountingDelegate->refCount();
		return m_refCount;
	}

private:
    atomic::AtomicLong m_refCount;
    nxpl::PluginInterface* m_objToWatch;
    CommonRefManager* m_refCountingDelegate;
};

template <typename T>
class CommonRefCounter: public T
{
public:
    CommonRefCounter(const CommonRefCounter&) = delete;
    CommonRefCounter& operator=(const CommonRefCounter&) = delete;
    CommonRefCounter(CommonRefCounter&&) = delete;
    CommonRefCounter& operator=(CommonRefCounter&&) = delete;
    virtual ~CommonRefCounter() = default;

    virtual unsigned int addRef() override { return m_refManager.addRef(); }
    virtual unsigned int releaseRef() override { return m_refManager.releaseRef(); }

	unsigned int refCount() const { return m_refManager.refCount(); }

protected:
    CommonRefManager m_refManager;

    CommonRefCounter(): m_refManager(static_cast<T*>(this)) {}
    CommonRefCounter(CommonRefManager* refManager): m_refManager(refManager) {}
};

/**
 * Intended for debug.
 * @return Reference counter, or 0 if object is null.
 */
template<typename Interface>
unsigned int refCount(Interface* object)
{
	if (const auto commonRefCounter = dynamic_cast<CommonRefCounter<Interface>*>(object))
		return commonRefCounter->refCount();

	return 0;
}

enum NxGuidFormatOption
{
    uppercase = 0x1,
    hyphens = 0x2,
    braces = 0x4,
    applyAll = uppercase | hyphens | braces
};

class NxGuidHelper
{
public:
    static nxpl::NX_GUID nullGuid()
    {
        return {{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}};
    }

    static nxpl::NX_GUID fromRawData(const char* data)
    {
        nxpl::NX_GUID result;
        memcpy(result.bytes, data, sizeof(result.bytes));
        return result;
    }

    static nxpl::NX_GUID fromStdString(const std::string& guidStr)
    {
        static const auto kMinGuidStrSize = 32;
        static const auto kGuidBytesNumber = 16;

        if (guidStr.size() < kMinGuidStrSize)
            return NxGuidHelper::nullGuid();

        nxpl::NX_GUID guid;
        int currentByteIndex = 0;
        std::string currentByteString;
        for (std::string::size_type i = 0; i < guidStr.size(); ++i)
        {
            if (guidStr[i] == '{' || guidStr[i] == '}' || guidStr[i] == '-'
                || guidStr[i] == '\t' || guidStr[i] == '\n' || guidStr[i] == 'r'
                || guidStr[i] == ' ')
            {
                continue;
            }

            if (currentByteIndex >= kGuidBytesNumber)
                return NxGuidHelper::nullGuid();

            currentByteString += guidStr[i];
            if (currentByteString.size() == 2)
            {
                char* pEnd = nullptr;
                errno = 0; //< Required before strtol().
                const long v = std::strtol(currentByteString.c_str(), &pEnd, /*base*/ 16);
                const bool hasError =  v > std::numeric_limits<unsigned char>::max()
                    || v < std::numeric_limits<unsigned char>::min()
                    || errno != 0
                    || *pEnd != '\0';

                if (hasError)
                    return NxGuidHelper::nullGuid();

                guid.bytes[currentByteIndex] = (unsigned char) v;
                ++currentByteIndex;
                currentByteString.clear();
            }
        }

        if (currentByteIndex != kGuidBytesNumber)
            return NxGuidHelper::nullGuid();

        return guid;
    }
};

static std::string toStdString(
    const nxpl::NX_GUID& guid,
    unsigned int format = NxGuidFormatOption::applyAll)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    if (format & NxGuidFormatOption::braces)
        ss << '{';

    if (format & NxGuidFormatOption::uppercase)
        ss << std::uppercase;

    for (int i = 0; i < 4; ++i)
    {
        ss << std::setw(2);
        ss << static_cast<unsigned int>(guid.bytes[i]);
    }

    if (format & NxGuidFormatOption::hyphens)
        ss << '-';

    for (int i = 0; i < 2; ++i)
    {
        ss << std::setw(2);
        ss << static_cast<unsigned int>(guid.bytes[4 + i]);
    }

    if (format & NxGuidFormatOption::hyphens)
        ss << "-";

    for (int i = 0; i < 2; ++i)
    {
        ss << std::setw(2);
        ss << static_cast<unsigned int>(guid.bytes[6 + i]);
    }

    if (format & NxGuidFormatOption::hyphens)
        ss << "-";

    for (int i = 0; i < 2; ++i)
    {
        ss << std::setw(2);
        ss << static_cast<unsigned int>(guid.bytes[8 + i]);
    }

    if (format & NxGuidFormatOption::hyphens)
        ss << "-";

    for (int i = 0; i < 6; ++i)
    {
        ss << std::setw(2);
        ss << static_cast<unsigned int>(guid.bytes[10 + i]);
    }

    if (format & NxGuidFormatOption::braces)
        ss << '}';

    return ss.str();
}

} // namespace nxpt

namespace nxpl {

inline bool operator==(const nxpl::NX_GUID& id1, const nxpl::NX_GUID& id2)
{
    return memcmp(id1.bytes, id2.bytes, sizeof(id1.bytes)) == 0;
}

inline std::ostream& operator<<(std::ostream& os, const nxpl::NX_GUID& id)
{
    return os << nxpt::toStdString(id);
}

} // namespace nxpl

namespace std {

template<>
struct hash<nxpl::NX_GUID>
{
    std::size_t operator()(const nxpl::NX_GUID& guid) const
    {
        std::size_t h;

        for (size_t i = 0; i < sizeof(guid.bytes); ++i)
            h = (h + (324723947 + guid.bytes[i])) ^ 93485734985;

        return h;
    }
};

} // namespace std

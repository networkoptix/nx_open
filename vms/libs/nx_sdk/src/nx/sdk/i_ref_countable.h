// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstring>
#include <cstdarg>
#include <cassert>
#include <type_traits>

#include <nx/sdk/ptr.h>

namespace nx {
namespace sdk {

class IRefCountable;

/**
 * Identifier of an interface, used for queryInterface().
 *
 * NOTE: For binary compatibility with plugins compiled with the old SDK, the binary layout of
 * this struct is identical to the layout of a pointer to the identifier bytes, and the length
 * of the identifier string should be not less than 15, because queryInterface() of the old SDK
 * received a const reference to a struct containing a 16-byte array. This also preserves
 * binary compatibility with plugins compiled using the old SDK (class PluginInterface).
 */
struct InterfaceId
{
    const char* const value; /**< Statically-allocated, neither null nor empty. */

    /** Enable initialization with a character array only. */
    template<int len>
    explicit constexpr InterfaceId(const char (&charArray)[len]): value(charArray)
    {
        static_assert(len + /*terminating zero*/ 1 >= 16,
            "Interface id should contain at least 15 chars");
        assert(value[0] != '\0');
        assert(value[1] != '\0');
        assert(value[2] != '\0');
        assert(value[3] != '\0');
        assert(value[4] != '\0');
        assert(value[5] != '\0');
        assert(value[6] != '\0');
        assert(value[7] != '\0');
        assert(value[8] != '\0');
        assert(value[9] != '\0');
        assert(value[10] != '\0');
        assert(value[11] != '\0');
        assert(value[12] != '\0');
        assert(value[13] != '\0');
        assert(value[14] != '\0');
    }

    /**
     * Makes a compound interface id for interface templates like IList<IItem>. Usage:
     * ```
     * static auto interfaceId()
     * {
     *     return InterfaceId::makeForTemplate<IList<IItem>, IItem>("nx::sdk::IList");
     * }
     * ```
     */
    template<class TemplateInstance, class TemplateArg>
    static InterfaceId makeForTemplate(const char* templateInterfaceIdBase)
    {
        static constexpr int kMaxStaticInterfaceIdSize = 256;

        static_assert(std::is_base_of<IRefCountable, TemplateInstance>::value,
            "TemplateInstance should be inherited from IRefCountable");
        static_assert(std::is_base_of<IRefCountable, TemplateArg>::value,
            "TemplateArg should be inherited from IRefCountable");

        // The id is stored in a static array because it is unique for the given template args.
        static char id[kMaxStaticInterfaceIdSize];
        assert(id[0] == '\0'); //< Assert that the static id has not been initialized yet.

        const int result = snprintf(
            id,
            kMaxStaticInterfaceIdSize,
            "%s<%s>",
            templateInterfaceIdBase,
            TemplateArg::interfaceId().value);

        /*unused*/ (void) result; //< If assert() is wiped out in Release, `result` will be unused.
        assert(result >= 15 && result < kMaxStaticInterfaceIdSize);

        return InterfaceId(id);
    }

    InterfaceId() = delete;

    bool operator==(const InterfaceId& other) const { return strcmp(value, other.value) == 0; }
    bool operator!=(const InterfaceId& other) const { return !(*this == other); }
};

//-------------------------------------------------------------------------------------------------

/**
 * Base for all interfaces - abstract classes with only pure-virtual non-overloaded functions, so
 * that an object implementing an interface can be passed by pointer between dynamic libraries
 * compiled using potentially different compilers and their standard libraries. Such abstraction
 * is needed to enable plugins. It is somewhat similar to Microsoft Component Object Model (COM).
 *
 * Interface methods are allowed to use only C-style input and output types (without STL classes),
 * because on each side of a dynamic library different implementations/versions of STL can be used.
 * The only ABI aspect interfaces rely upon - size of pointer (64-bit) and order of methods in VMT
 * (in the order of declaration, base class methods first). Overloaded methods are not allowed
 * because their order in VMS can be different across compilers. Also dynamic_cast can not be used
 * for such objects, because data structures behind the VMT can be different across compilers.
 *
 * Interfaces may contain type definitions and non-virtual inline utility methods (they will be
 * compiled by each side independently).
 *
 * Each object implementing an interface should have an internal reference counter, which, when
 * decremented to zero, triggers object deletion.
 *
 * To provide dynamic-cast-like behavior, and more generally, the ability to yield objects
 * implementing other interfaces (not necessarily the same object as the one being queried), each
 * object must implement queryInterface() that takes an interface identifier (value) and returns
 * a pointer to the object implementing the requested interface.
 *
 * General rules for passing objects with reference counting:
 * - If a function returns a pointer to an interface, it should increment the counter prior to
 *     returning, thus, the counter should be decremented by the caller after using the object.
 * - If a function receives a pointer to an interface as an argument, it should not decrement its
 *     counter, and should not increment it (unless it needs the object to survive this call).
 * - If a class owns an object in a field, it should decrement its counter in the destructor.
 *
 * NOTE: For binary compatibility with old plugins, the VMT layout of this interface (4 entries -
 * destructor, queryInterface(), addRef(), releaseRef()) should not change, as well as the binary
 * prototypes (arguments and return values) of its entries.
 */
class IRefCountable
{
public:
    /** Each derived interface is expected to implement such static method with its own string. */
    static auto interfaceId() { return InterfaceId("nx::sdk::IRefCountable"); }

    /** VMT #0. */
    virtual ~IRefCountable() = default;

protected:
    /**
     * VMT #1.
     *
     * Intended to be called indirectly, via queryInterface<Interface>(), hence `protected`.
     * @return Object that requires releaseRef() by the caller when it no longer needs it, or null
     *     if the requested interface is not implemented.
     */
    virtual IRefCountable* queryInterface(InterfaceId id)
    {
        if (id == interfaceId())
        {
            addRef();
            return this;
        }
        return nullptr;
    }

public:
    template<class Interface>
    Ptr<Interface> queryInterface()
    {
        return toPtr(static_cast<Interface*>(queryInterface(Interface::interfaceId())));
    }

    template<class Interface>
    Ptr<const Interface> queryInterface() const
    {
        return toPtr(static_cast<const Interface*>(
            const_cast<IRefCountable*>(this)->queryInterface(Interface::interfaceId())));
    }

    /**
     * VMT #2.
     * @return New reference count.
     */
    virtual int addRef() const = 0;

    /**
     * VMT #3.
     * @return New reference count, having deleted the object if it reaches zero.
     */
    virtual int releaseRef() const = 0;

    /**
     * Intended for debug. Is not thread-safe, because does addRef()/releaseRef() pair of calls.
     * @return Reference counter, or 0 if the pointer is null.
     */
    int refCountThreadUnsafe() const
    {
        const void* this_ = this; //< Suppress warning that `this` cannot be null.
        if (this_ == nullptr)
            return 0;

        /*unused*/ (void) addRef();
        return releaseRef();
    }
};

} // namespace sdk
} // namespace nx

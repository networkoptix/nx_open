#pragma once

#include <cstring>

namespace nx {
namespace sdk {

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
        }

        InterfaceId() = delete;

        bool operator==(const InterfaceId& other) const { return strcmp(value, other.value) == 0; }
        bool operator!=(const InterfaceId& other) const { return !(*this == other); }
    };

    /** Each derived interface is expected to implement such static method with its own string. */
    static auto interfaceId() { return InterfaceId("nx::sdk::IRefCountable"); }

    /** VMT #0. */
    virtual ~IRefCountable() = default;

    /**
     * VMT #1.
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

    const IRefCountable* queryInterface(InterfaceId id) const
    {
        return const_cast<IRefCountable*>(this)->queryInterface(id);
    }

    template<class Interface>
    Interface* queryInterface()
    {
        return static_cast<Interface*>(queryInterface(Interface::interfaceId()));
    }

    template<class Interface>
    const Interface* queryInterface() const
    {
        return static_cast<const Interface*>(queryInterface(Interface::interfaceId()));
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
        if (this == nullptr)
            return 0;

        /*unused*/ (void) addRef();
        return releaseRef();
    }
};

} // namespace sdk
} // namespace nx

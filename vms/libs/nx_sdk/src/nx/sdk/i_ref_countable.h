// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstring>
#include <string>
#include <type_traits>

#include <nx/sdk/ptr.h>

namespace nx::sdk {

/**
 * Base for all interfaces - abstract classes with only pure-virtual non-overloaded functions, so
 * that an object implementing an interface can be passed by pointer between dynamic libraries
 * compiled using potentially different compilers and their standard libraries. Such abstraction
 * is needed to enable plugins. It is somewhat similar to Microsoft Component Object Model (COM).
 *
 * Interface methods are allowed to use only C-style input and output types (without STL classes),
 * because on each side of a dynamic library different implementations/versions of STL can be used.
 * The only ABI aspects that interfaces rely upon - size of pointer, and order of methods in a VMT
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
 * object must implement queryInterface() which takes an interface identifier and returns a pointer
 * to the object implementing the requested interface.
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
     * Identifier of an interface, used for queryInterface(). A pointer to this struct is actually
     * a pointer to the string with an interface id - the struct is needed only to protect from
     * constructing incorrect interface id, and from passing something else to queryInterface().
     *
     * NOTE: For binary compatibility with plugins compiled with the old SDK, the binary layout of
     * this struct is an array of chars with length not less than 16, because queryInterface() of
     * the old SDK received a const reference to a struct containing a 16-byte array.
     */
    struct InterfaceId
    {
        static constexpr int kMinSize = 16; //< For compatibility with the old SDK.
        static constexpr int minSize() { return kMinSize; } //< For C++14, to avoid the definition.

        char value[kMinSize];

        InterfaceId() = delete; //< No instances - only InterfaceId* are used via reinterpret_cast.

        bool operator==(const InterfaceId& other) const { return strcmp(value, other.value) == 0; }
        bool operator!=(const InterfaceId& other) const { return !(*this == other); }
    };

protected:
    /** Intended to be used in interfaceId(). Can be called only with a string literal. */
    template<int len>
    static constexpr const InterfaceId* makeId(const char (&charArray)[len])
    {
        static_assert(len + /*terminating \0*/ 1 >= InterfaceId::minSize(),
            "Interface id is too short");

        return reinterpret_cast<const InterfaceId*>(charArray);
    }

public:
    /** Each derived interface is expected to implement such static method with its own string. */
    static auto interfaceId() { return makeId("nx::sdk::IRefCountable"); }

    /** VMT #0. */
    virtual ~IRefCountable() = default;

protected:
    /**
     * Makes a compound interface id for interface templates like IList<IItem>. Usage:
     * ```
     * static auto interfaceId()
     * {
     *     return IList::template makeIdForTemplate<IList<IItem>, IItem>("nx::sdk::IList");
     * }
     * ```
     */
    template<class TemplateInstance, class TemplateArg, int len>
    static const InterfaceId* makeIdForTemplate(const char (&baseIdCharArray)[len])
    {
        static_assert(len + /*angle brackets*/ 2 + /*terminating \0*/ 1 >= InterfaceId::minSize(),
            "Base part of interface id is too short");
        static_assert(std::is_base_of<IRefCountable, TemplateInstance>::value,
            "TemplateInstance must be inherited from IRefCountable");
        static_assert(std::is_base_of<IRefCountable, TemplateArg>::value,
            "TemplateArg must be inherited from IRefCountable");

        static const std::string id =
            std::string(&baseIdCharArray[0]) + "<" + &TemplateArg::interfaceId()->value[0] + ">";

        return reinterpret_cast<const InterfaceId*>(id.c_str());
    }

    /**
     * VMT #1.
     *
     * Intended to be called indirectly, via queryInterface<Interface>(), hence `protected`.
     * @return Object that requires releaseRef() by the caller when it no longer needs it, or null
     *     if the requested interface is not implemented.
     */
    virtual IRefCountable* queryInterface(const InterfaceId* id)
    {
        if (*interfaceId() == *id)
        {
            addRef();
            return this;
        }
        return nullptr;
    }

    /** Intended to be used via hasAlternativeInterfaceId. */
    template<typename RefCountable>
    struct HasAlternativeInterfaceId
    {
    private:
        // SFINAE: The first overload will be chosen when T::alternativeInterfaceId() exists,
        // the second overload will be chosen otherwise. Since test() is used only in decltype(),
        // it does not need a function definition.

        template<typename T>
        static constexpr std::true_type test(decltype(T::alternativeInterfaceId())* arg);

        template<typename T>
        static constexpr std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<RefCountable>(nullptr))::value;
    };

    /**
     * Checks whether the specified interface class has alternativeInterfaceId(). Usage example:
     * ```
     *     if constexpr (hasAlternativeInterfaceId<ISomething>)
     *         return something->queryInterface(ISomething::alternativeInterfaceId());
     * ```
     */
    template<typename RefCountable>
    static constexpr bool hasAlternativeInterfaceId = HasAlternativeInterfaceId<RefCountable>::value;

public:
    template<class RefCountable>
    Ptr<RefCountable> queryInterface()
    {
        IRefCountable* refCountable = nullptr;
        if constexpr (hasAlternativeInterfaceId<RefCountable>)
            refCountable = queryInterface(RefCountable::alternativeInterfaceId());
        if (!refCountable)
            refCountable = queryInterface(RefCountable::interfaceId());
        return Ptr(static_cast<RefCountable*>(refCountable));
    }

    template<class RefCountable>
    Ptr<const RefCountable> queryInterface() const
    {
        auto nonConstThis = const_cast<IRefCountable*>(this);
        IRefCountable* refCountable = nullptr;
        if constexpr (hasAlternativeInterfaceId<RefCountable>)
            refCountable = nonConstThis->queryInterface(RefCountable::alternativeInterfaceId());
        if (!refCountable)
            refCountable = nonConstThis->queryInterface(RefCountable::interfaceId());
        return Ptr(static_cast<const RefCountable*>(refCountable));
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

} // namespace nx::sdk

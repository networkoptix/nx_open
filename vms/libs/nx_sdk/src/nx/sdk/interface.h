// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstring>

#include <nx/sdk/uuid.h>

#include "i_ref_countable.h"

namespace nx::sdk {

/**
 * Helper class to define interfaces: provides the appropriate queryInterface().
 *
 * Usage:
 * ```
 * class MyInterface: public nx::sdk::Interface<MyInterface, MyBaseInterface>
 * {
 * public:
 *     static auto interfaceId() { return makeId("my_namespace::MyInterface"); }
 *     ... // pure virtual methods
 * };
 * ```
 *
 * NOTE: For binary compatibility, when an interface needs to be extended with new methods, instead
 * of adding virtual methods to the interface class, define a new descendant class with the new
 * methods, and use the following naming scheme for class names and interface ids:
 * ```
 * // Version 0 (the original):
 * class ISomething("my_namespace::ISomething") { <original-virtual-methods> }
 *
 * // Version 1 (first evolution step):
 * class ISomething0("my_namespace::ISomething") { <original-virtual-methods> }
 * class ISomething("my_namespace::ISomething1") { <step1-added-virtual-methods> }
 *
 * // Version 2 (second evolution step):
 * class ISomething0("my_namespace::ISomething") { <original-virtual-methods> }
 * class ISomething1("my_namespace::ISomething1") { <step1-added-virtual-methods> }
 * class ISomething("my_namespace::ISomething2") { <step2-added-virtual-methods> }
 * ```
 * Such naming scheme implies the following:
 * - The interface id for the particular VMT never changes, though the C++ class may be renamed.
 * - If both the interface id and the interface class have a number appended, the numbers coincide.
 * - The class of the latest interface version always has the original name without a number.
 */
template<class DerivedInterface, class BaseInterface = IRefCountable>
class Interface: public BaseInterface
{
public:
    using IRefCountable::queryInterface; //< Needed to enable overloaded template versions.

protected:
    virtual IRefCountable* queryInterface(const IRefCountable::InterfaceId* id) override
    {
        return doQueryInterface(id);
    }

    /** Call from DerivedInterface::queryInterface() to support interface id from the old SDK. */
    IRefCountable* queryInterfaceSupportingDeprecatedId(
        const IRefCountable::InterfaceId* id,
        const Uuid& deprecatedInterfaceId)
    {
        static_assert(Uuid::size() == IRefCountable::InterfaceId::minSize(),
            "Broken compatibility with old SDK");
        if (memcmp(id, deprecatedInterfaceId.data(), Uuid::size()) == 0)
        {
            this->addRef();
            // The cast is needed to shift the pointer in case of multiple inheritance.
            return static_cast<DerivedInterface*>(this);
        }
        return doQueryInterface(id);
    }

private:
    static_assert(std::is_base_of<IRefCountable, BaseInterface>::value,
        "Template parameter BaseInterface should be derived from IRefCountable");

    /**
     * A private constructor is needed to statically assure that only DerivedInterface (which is
     * made the only friend) can inherit from this class.
     */
    Interface()
    {
        // Assure that DerivedInterface has interfaceId().
        (void) &DerivedInterface::interfaceId;
    }

    friend DerivedInterface;

    IRefCountable* doQueryInterface(const IRefCountable::InterfaceId* id)
    {
        for (const auto& ownId: IRefCountable::alternativeInterfaceIds(DerivedInterface::interfaceId()))
        {
            if (*ownId == *id)
            {
                this->addRef();
                // The cast is needed to shift the pointer in case of multiple inheritance.
                return static_cast<DerivedInterface*>(this);
            }
        }
        return BaseInterface::queryInterface(id);
    }
};

} // namespace nx::sdk

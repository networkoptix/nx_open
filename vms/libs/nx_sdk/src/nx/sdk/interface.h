// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstring>

#include <nx/sdk/uuid.h>

#include "i_ref_countable.h"

namespace nx {
namespace sdk {

/**
 * Helper class to define interfaces: provides the appropriate queryInterface().
 *
 * Usage:
 * ```
 * class MyInterface: public nx::sdk::Interface<MyInterface, MyBaseInterface>
 * {
 * public:
 *     static auto interfaceId() { return interfaceId("my_namespace::MyInterface"); }
 *     ... // pure virtual methods
 * };
 * ```
 */
template<class DerivedInterface, class BaseInterface = IRefCountable>
class Interface: public BaseInterface
{
public:
    using IRefCountable::queryInterface; //< Needed to enable overloaded template versions.

protected:
    using IRefCountable::makeIdForTemplate; //< Needed to make the method available in descendants.

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

    // Statically assure that DerivedInterface is inherited from this class.
    Interface() = default;
    friend DerivedInterface;

    IRefCountable* doQueryInterface(const IRefCountable::InterfaceId* id)
    {
        if (*DerivedInterface::interfaceId() == *id)
        {
            this->addRef();
            // The cast is needed to shift the pointer in case of multiple inheritance.
            return static_cast<DerivedInterface*>(this);
        }
        return BaseInterface::queryInterface(id);
    }
};

} // namespace sdk
} // namespace nx

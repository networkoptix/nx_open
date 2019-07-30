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
 *     static auto interfaceId() { return InterfaceId("my_namespace::MyInterface"); }
 *     ... // pure virtual methods
 * };
 * ```
 */
template<class DerivedInterface, class BaseInterface = IRefCountable>
class Interface: public BaseInterface
{
private:
    static_assert(std::is_base_of<IRefCountable, BaseInterface>::value,
        "Template parameter BaseInterface should be derived from IRefCountable");

    // Statically assure that DerivedInterface is inherited from this class.
    Interface() = default;
    friend DerivedInterface;

public:
    using IRefCountable::queryInterface; //< Enable const overload.

    virtual IRefCountable* queryInterface(InterfaceId id) override
    {
        return doQueryInterface(id);
    }

protected:
    /** Call from DerivedInterface::queryInterface() to support interface id from the old SDK. */
    IRefCountable* queryInterfaceSupportingDeprecatedId(
        InterfaceId id,
        const Uuid& deprecatedInterfaceId)
    {
        if (memcmp(id.value, deprecatedInterfaceId.data(), Uuid::kSize) == 0)
        {
            this->addRef();
// TODO: #mshevchenko: Try to remove the cast.
            return static_cast<DerivedInterface*>(this);
        }
        return doQueryInterface(id);
    }

private:
    IRefCountable* doQueryInterface(InterfaceId id)
    {
        if (id == DerivedInterface::interfaceId())
        {
            this->addRef();
// TODO: #mshevchenko: Try to remove the cast.
            return static_cast<DerivedInterface*>(this);
        }
        return BaseInterface::queryInterface(id);
    }
};

} // namespace sdk
} // namespace nx

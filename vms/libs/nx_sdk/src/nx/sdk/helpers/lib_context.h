// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <memory>

#include <nx/sdk/helpers/i_ref_countable_registry.h>

namespace nx {
namespace sdk {

/**
 * TODO: #mshevchenko Document, and write about not using IRefCountable.
 */
class ILibContext
{
public:
    virtual ~ILibContext() = default;
    virtual void setName(const char* name) = 0;
    virtual void setRefCountableRegistry(IRefCountableRegistry* refCountableRegistry) = 0;
};

/**
 * TODO: #mshevchenko Document.
 */
class LibContext final: public ILibContext
{
public:
    LibContext();
    virtual ~LibContext() override;

    virtual void setName(const char* name) override;
    virtual void setRefCountableRegistry(IRefCountableRegistry* refCountableRegistry) override;

    const std::string& name() const { return m_name; }

    /** @return Null if the registry has not been set. */
    IRefCountableRegistry* refCountableRegistry() { return m_refCountableRegistry.get(); }

private:
    std::string m_name = "unnamed_lib_context";
    std::unique_ptr<IRefCountableRegistry> m_refCountableRegistry;
};

//-------------------------------------------------------------------------------------------------

/**
 * TODO: #mshevchenko Document.
 */
LibContext& libContext();

static constexpr const char* kNxLibContextFuncName = "nxLibContext";
typedef ILibContext* (*NxLibContextFunc)();

#if !defined(NX_SDK_API)
    #if !defined(NX_PLUGIN_API)
        #error "Either NX_SDK_API or NX_PLUGIN_API macro should be defined to export a function."
    #endif
    #define NX_SDK_API NX_PLUGIN_API
#endif

/**
 * TODO: #mshevchenko Document.
 */
extern "C" NX_SDK_API ILibContext* nxLibContext();

} // namespace sdk
} // namespace nx

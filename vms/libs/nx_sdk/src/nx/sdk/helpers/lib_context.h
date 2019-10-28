// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <memory>
#include <mutex>

#include <nx/sdk/helpers/i_ref_countable_registry.h>

namespace nx {
namespace sdk {

/**
 * Interface to LibContext which is used by the Server to set up the context for each plugin
 * dynamic library.
 */
class ILibContext
{
public:
    virtual ~ILibContext() = default;

    virtual void setName(const char* name) = 0;
    virtual void setRefCountableRegistry(IRefCountableRegistry* refCountableRegistry) = 0;
};

/**
 * Context which has a dedicated instance in each plugin dynamic library, and one more instance in
 * the Server. Provides some services to a library which uses the SDK (a plugin or the Server),
 * like detecting memory leaks of ref-countable objects, or keeping a name to be used for the
 * logging prefix in the library.
 */
class LibContext final: public ILibContext
{
public:
    /**
     * For the LibContext of a plugin, called by the Server immediately after loading the plugin
     * dynamic library. For the LibContext of the Server, called before any involving of the SDK.
     */
    virtual void setName(const char* name) override;

    /**
     * Called by the Server after setName().
     * @param refCountableRegistry Will be deleted in the LibContext destructor. Can be null if
     *     the leak detection is not enabled.
     */
    virtual void setRefCountableRegistry(IRefCountableRegistry* refCountableRegistry) override;

    const std::string& name() const { return m_name; }

    /** @return Null if the registry has not been set. */
    IRefCountableRegistry* refCountableRegistry() const { return m_refCountableRegistry.get(); }

private:
    std::string m_name = "unnamed_lib_context";
    std::unique_ptr<IRefCountableRegistry> m_refCountableRegistry;
    std::mutex m_mutex;
};

//-------------------------------------------------------------------------------------------------

/**
 * Holds the LibContext instance in a static variable. Should be called to access the context of
 * the current dynamic library.
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
 * Should be called only by the Server via resolving by name in a loaded plugin dynamic library.
 *
 * ATTENTION: If called directly from a C++ code, a random instance of this function will be
 * actually called (possibly belonging to a different plugin) because of the dynamic library
 * runtime linking algorithm.
 */
extern "C" NX_SDK_API ILibContext* nxLibContext();

} // namespace sdk
} // namespace nx

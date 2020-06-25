// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lib_context.h"

#define NX_PRINT_PREFIX "[nx::sdk::LibContext] "
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {

void LibContext::setName(const char* name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!NX_KIT_ASSERT(m_name == kDefaultName,
        nx::kit::utils::format("Attempt to change LibContext name from %s to %s.",
            nx::kit::utils::toString(m_name).c_str(), nx::kit::utils::toString(name).c_str())))
    {
        return;
    }

    if (!NX_KIT_ASSERT(name) || !NX_KIT_ASSERT(name[0] != '\0'))
    {
        m_name = "incorrectly_named_lib_context";
        return;
    }

    m_name = name;
}

void LibContext::setRefCountableRegistry(IRefCountableRegistry* refCountableRegistry)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!NX_KIT_ASSERT(!m_refCountableRegistry,
        "LibContext refCountableRegistry has already been set."))
    {
        return;
    }

    m_refCountableRegistry.reset(refCountableRegistry);
}

//-------------------------------------------------------------------------------------------------

LibContext& libContext()
{
    static LibContext instance;
    return instance;
}

/*extern "C"*/ ILibContext* nxLibContext()
{
    return &libContext();
}

/*extern "C"*/ const char* nxSdkVersion()
{
    static constexpr char str[] =
        #include <nx_sdk_version.inc>
    ;

    static_assert(sizeof(str) > 1, "nx_sdk_version.inc must contain a non-empty string literal.");
    return str;
}

} // namespace sdk
} // namespace nx

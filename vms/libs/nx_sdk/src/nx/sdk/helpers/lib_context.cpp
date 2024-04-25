// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lib_context.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[nx::sdk::LibContext] "
#include <nx/kit/debug.h>

namespace nx::sdk {

void LibContext::setName(const char* name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!NX_KIT_ASSERT(m_name == kDefaultName || m_name == std::string(name),
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

const char* sdkVersion()
{
    // The value placed in nx_sdk_version.inc reflects the version of the VMS, in format
    // "<major>.<minor>.<patch> <meta-version>". Note that the build number is intentionally left
    // out, because various VMS builds (e.g. different customizations) must yield identical SDK
    // packages, thus, nx-sdk-version must be the same.
    static constexpr char str[] =
        #include <nx_sdk_version.inc>
    ;

    static_assert(sizeof(str) > 1, "nx_sdk_version.inc must contain a non-empty string literal.");
    return str;
}

/*extern "C"*/ const char* nxSdkVersion()
{
    return sdkVersion();
}

std::map<std::string, std::string>& unitTestOptions()
{
    static std::map<std::string, std::string> instance;
    return instance;
}

/*extern "C"*/ void nxSetUnitTestOptions(const IStringMap* options)
{
    if (!options)
        return;

    unitTestOptions().clear();
    for (int i = 0; i < options->count(); ++i)
        unitTestOptions().emplace(options->key(i), options->value(i));
}

} // namespace nx::sdk

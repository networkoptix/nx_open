// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lib_context.h"

#define NX_PRINT_PREFIX "[nx::sdk::LibContext] "
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {

void LibContext::setName(const char* name)
{
    NX_KIT_ASSERT(name);
    NX_KIT_ASSERT(name[0] != '\0');
    m_name = name;
}

void LibContext::setRefCountableRegistry(IRefCountableRegistry* refCountableRegistry)
{
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

} // namespace sdk
} // namespace nx

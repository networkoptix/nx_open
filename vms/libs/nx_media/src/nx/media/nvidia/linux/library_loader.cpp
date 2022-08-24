// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef __linux__

#include "library_loader.h"

#include <dlfcn.h>

#include <nx/utils/log/log.h>

namespace nx::media::nvidia::linux {

LibraryLoader::~LibraryLoader()
{
    dlclose(m_libraryHandle);
}

bool LibraryLoader::load(const char * name)
{
    dlerror();
    m_libraryHandle = dlopen(name, RTLD_GLOBAL | RTLD_NOW);
    if (nullptr == m_libraryHandle)
    {
        NX_WARNING(this, "Failed to load library: %1", name);
        return false;
    }
    return true;
}

void* LibraryLoader::getFunction(const char* name)
{
    auto result = dlsym(m_libraryHandle, name);
    const char* error = dlerror();
    if (error)
    {
        NX_WARNING(this, "Failed to load symbol: %1, error: %2", name, error);
        dlclose(m_libraryHandle);
        return nullptr;
    }
    if (!result)
        NX_WARNING(this, "Failed to get function: %1", name);
    return result;
}

} // nx::media::quick_sync::linux

#endif // __linux__
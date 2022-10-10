// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "library_loader.h"

#include <nx/utils/log/log.h>

#ifdef __linux__

#include <dlfcn.h>

namespace nx::media::nvidia {

LibraryLoader::~LibraryLoader()
{
    dlclose(m_libraryHandle);
}

bool LibraryLoader::load(const char* name)
{
    m_libraryHandle = dlopen(name, RTLD_GLOBAL | RTLD_NOW);
    if (m_libraryHandle == nullptr)
    {
        NX_WARNING(this, "Failed to load library: %1", name);
        return false;
    }
    return true;
}

void* LibraryLoader::getFunction(const char* name)
{
    auto result = dlsym(m_libraryHandle, name);
    if (const char* error = dlerror())
    {
        NX_WARNING(this, "Failed to load symbol: %1, error: %2", name, error);
        dlclose(m_libraryHandle);
        return nullptr;
    }
    if (!result)
        NX_WARNING(this, "Failed to get function: %1", name);
    return result;
}

} // nx::media::nvidia

#elif _WIN32

namespace nx::media::nvidia {

LibraryLoader::~LibraryLoader()
{
    FreeLibrary(m_libraryHandle);
}

bool LibraryLoader::load(const char* name)
{
    m_libraryHandle = LoadLibraryA(name);
    if (nullptr == m_libraryHandle)
    {
        NX_WARNING(this, "Failed to load library: %1", name);
        return false;
    }
    return true;
}

void* LibraryLoader::getFunction(const char* name)
{
    void* const result = GetProcAddress(m_libraryHandle, name);
    if (!result)
    {
        NX_WARNING(this, "Failed to load symbol: %1", name);
        FreeLibrary(m_libraryHandle);
        return nullptr;
    }
    return result;
}

} // namespace nx::media::nvidia

#endif // _WIN32


// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#if defined(__linux__)

namespace nx::media::nvidia {

class LibraryLoader
{
public:
    ~LibraryLoader();
    bool load(const char* name);
    void* getFunction(const char* name);

private:
    void* m_libraryHandle = nullptr;
};

} // nx::media::nvidia

#elif defined(_WIN32)

#include <Windows.h>

namespace nx::media::nvidia {

class LibraryLoader
{
public:
    ~LibraryLoader();
    bool load(const char* name);
    void* getFunction(const char* name);

private:
    HMODULE m_libraryHandle = nullptr;
};

} // nx::media::nvidia

#endif // __linux__

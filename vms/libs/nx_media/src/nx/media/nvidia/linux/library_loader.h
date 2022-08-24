// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef __linux__

namespace nx::media::nvidia::linux {

class LibraryLoader
{
public:
    ~LibraryLoader();
    bool load(const char * name);
    void* getFunction(const char* name);

private:
    void* m_libraryHandle = nullptr;
};

} // nx::media::quick_sync::linux

#endif // __linux__
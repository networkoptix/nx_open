// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <Windows.h>

namespace nx::utils {

class NamedPipeSocketImpl
{
public:
    static constexpr DWORD BUFSIZE = 4*1024;

    HANDLE hPipe = INVALID_HANDLE_VALUE;
    bool onServerSide = false;
};

} // namespace nx::utils

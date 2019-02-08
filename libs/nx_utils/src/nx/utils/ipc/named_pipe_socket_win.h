#pragma once

#include <Windows.h>

class NamedPipeSocketImpl
{
public:
    static const DWORD BUFSIZE = 4*1024;

    HANDLE hPipe;
    bool onServerSide;

    NamedPipeSocketImpl();
};

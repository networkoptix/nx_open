////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef NAMED_PIPE_SOCKET_WIN32_H
#define NAMED_PIPE_SOCKET_WIN32_H

#ifdef _WIN32

#include <Windows.h>


class NamedPipeSocketImpl
{
public:
    static const DWORD BUFSIZE = 4*1024;

    HANDLE hPipe;

    NamedPipeSocketImpl();
};

#endif

#endif  //NAMED_PIPE_SOCKET_WIN32_H

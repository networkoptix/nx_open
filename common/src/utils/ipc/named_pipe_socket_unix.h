////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef NAMED_PIPE_SOCKET_UNIX_H
#define NAMED_PIPE_SOCKET_UNIX_H

#ifndef _WIN32

class NamedPipeSocketImpl
{
public:
    static const size_t BUFSIZE = 4*1024;

    int hPipe;

    NamedPipeSocketImpl();
};

#endif

#endif  //NAMED_PIPE_SOCKET_UNIX_H

#pragma once

class NamedPipeSocketImpl
{
public:
    static const size_t BUFSIZE = 4*1024;

    int hPipe;

    NamedPipeSocketImpl();
};

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::utils {

class NamedPipeSocketImpl
{
public:
    static const size_t BUFSIZE = 4*1024;

    int hPipe = -1;
};

} // namespace nx::utils

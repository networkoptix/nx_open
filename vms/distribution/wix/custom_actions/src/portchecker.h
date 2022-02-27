// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

class PortChecker
{
public:
    PortChecker();

    bool isPortAvailable(unsigned short port);

private:
    int fillBusyPorts();

private:
    std::set<unsigned short> ports;
};

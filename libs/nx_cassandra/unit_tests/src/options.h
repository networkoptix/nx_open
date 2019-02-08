#pragma once

#include <string>

struct Options
{
    std::string host;

    Options():
        host("127.0.0.1")
    {}
};

Options* options();
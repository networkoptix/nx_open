#pragma once

#include <ostream>

#include <nx/utils/argument_parser.h>

namespace nx::cctu {

class RelayLoadTest
{
public:
    class Settings
    {
    public:
        Settings(const nx::utils::ArgumentParser& args);

        std::string relayUrl;
        unsigned int count = 1;
    };

    static constexpr char kModeName[] = "relay-load-test";

    static void printOptions(std::ostream* output);
    static int run(const nx::utils::ArgumentParser& args);
};

} // namespace nx::cctu

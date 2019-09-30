#pragma once

#include <chrono>
#include <ostream>
#include <string>

#include <nx/utils/argument_parser.h>

namespace nx::cloud::db::client {

class LoadTest
{
public:
    static int establishManyConnections(
        const std::string& cdbUrl,
        const std::string& login,
        const std::string& password,
        const nx::utils::ArgumentParser& args);

    static int makeApiRequests(
        const std::string& cdbUrl,
        const std::string& login,
        const std::string& password,
        int connectionCount);

    static void printHelp(std::ostream& os);
};

} // namespace nx::cloud::db::client

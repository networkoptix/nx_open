#pragma once

#include <chrono>
#include <string>

namespace nx {
namespace cdb {
namespace client {

int establishManyConnections(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    int connectionCount,
    std::chrono::milliseconds maxDelayBeforeConnect);

int makeApiRequests(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    int connectionCount,
    std::chrono::milliseconds maxDelayBeforeConnect);

} // namespace client
} // namespace cdb
} // namespace nx

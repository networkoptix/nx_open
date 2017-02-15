#pragma once

namespace nx {
namespace cdb {
namespace client {

int establishManyConnections(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password);

} // namespace client
} // namespace cdb
} // namespace nx

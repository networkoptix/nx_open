#pragma once

#include <string>

namespace nx {
namespace cdb {
namespace client {

int generateSystems(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    int testSystemsToGenerate);

} // namespace client
} // namespace cdb
} // namespace nx

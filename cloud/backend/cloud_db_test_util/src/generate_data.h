#pragma once

#include <string>

namespace nx::cloud::db::client {

int generateSystems(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    int testSystemsToGenerate);

} // namespace nx::cloud::db::client

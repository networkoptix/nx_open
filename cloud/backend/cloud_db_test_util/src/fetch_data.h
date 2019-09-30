#pragma once

#include <string>

namespace nx::cloud::db::client {

int fetchData(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    const std::string& fetchRequest);

} // namespace nx::cloud::db::client

#pragma once

namespace nx {
namespace cdb {
namespace client {

int fetchData(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    const std::string& fetchRequest);

} // namespace client
} // namespace cdb
} // namespace nx

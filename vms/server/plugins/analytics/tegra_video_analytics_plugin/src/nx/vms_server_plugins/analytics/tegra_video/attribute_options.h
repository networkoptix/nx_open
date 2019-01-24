#pragma once

#include <map>
#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace tegra_video {

static const std::map<std::string, std::vector<std::string>> kCarAttributeOptions = {
    {"Object Type", {"Car"}},
    {"Vendor", {
        "Nissan",
        "Toyota",
        "Mercedes",
        "Audi",
        "Citroen",
        "VAZ",
        "Fiat",
        "Dodge",
        "BMW",
        "Great Wall"
    }}
};

static const std::map<std::string, std::vector<std::string>> kHumanAttributeOptions = {
    {"Object Type", {"Human"}},
    {"Age", {"Adult", "Child"}}
};

} // namespace tegra_video
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

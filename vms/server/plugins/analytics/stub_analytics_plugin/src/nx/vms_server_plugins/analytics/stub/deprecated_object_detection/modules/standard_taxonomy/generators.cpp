// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generators.h"

#include <vector>
#include <random>
#include <iostream>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

std::vector<std::string> kCarVendors = {
    "Honda",
    "Mercedes",
    "BMW",
    "Lada",
    "Toyota",
};

std::vector<std::string> kCarModels = {
    "Model A",
    "Model B",
    "Model C",
    "Model D",
    "Model E",
};

static const char numbers[] = "0123456789";
static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

std::string generateVehicleVendor()
{
    return kCarVendors[rand() % kCarVendors.size()];
}

std::string generateVehicleModel()
{
    return kCarModels[rand() % kCarModels.size()];
}

std::string generateLicensePlate()
{
    std::string result;

    result += letters[rand() % (sizeof(letters) - 1)];
    result += numbers[rand() % (sizeof(numbers) - 1)];
    result += numbers[rand() % (sizeof(numbers) - 1)];
    result += numbers[rand() % (sizeof(numbers) - 1)];
    result += letters[rand() % (sizeof(letters) - 1)];
    result += letters[rand() % (sizeof(letters) - 1)];

    return result;
}

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
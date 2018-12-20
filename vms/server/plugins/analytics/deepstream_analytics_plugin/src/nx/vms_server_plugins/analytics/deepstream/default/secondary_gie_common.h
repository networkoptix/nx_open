#pragma once

#include <vector>

#include <nx/gstreamer/property.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

static const int kFirstClassifier = 0;
static const int kSecondClassifier = 1;
static const int kThirdClassifier = 2;
static const int kMaxClassifiers = 3;

std::vector<nx::gstreamer::Property> giePropertiesByIndex(int index);

bool isSecondaryGieEnabled(int index);

std::string labelFileByIndex(int index);

std::string labelTypeByIndex(int index);

int uniqueIdByIndex(int index);

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

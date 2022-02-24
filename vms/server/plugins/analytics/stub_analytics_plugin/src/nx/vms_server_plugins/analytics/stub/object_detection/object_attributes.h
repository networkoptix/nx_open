// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <map>
#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

extern const std::map<
    /*objectTypeId*/ std::string,
    std::map</*attributeName*/ std::string, /*attributeValue*/ std::string>> kObjectAttributes;

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

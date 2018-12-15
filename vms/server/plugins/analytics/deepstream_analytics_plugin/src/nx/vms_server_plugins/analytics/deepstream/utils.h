#pragma once

#include <vector>
#include <string>
#include <chrono>

#include <plugins/plugin_api.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

static nxpl::NX_GUID kNullGuid = {{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

std::vector<std::string> split(const std::string& str, const std::string& delimiter);

std::vector<std::string> split(const std::string& str, const char delimiter);

std::string trimCopy(std::string str);

std::string* trim(std::string* inOutStr);

nxpl::NX_GUID makeGuid();

bool isNull(const nxpl::NX_GUID& guid);

std::string makeElementName(
    const std::string& pipelineName,
    const std::string& factoryName,
    const std::string& elementName);

std::chrono::milliseconds now();

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

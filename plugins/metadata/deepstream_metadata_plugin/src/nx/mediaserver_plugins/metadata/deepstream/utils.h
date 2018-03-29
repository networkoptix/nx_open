#pragma once

#include <vector>
#include <string>

#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
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

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

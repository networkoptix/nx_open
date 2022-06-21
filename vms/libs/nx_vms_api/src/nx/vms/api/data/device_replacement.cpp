// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_replacement.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceReplacementRequest,
    (json),
    DeviceReplacementRequest_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceReplacementInfo,
    (json),
    DeviceReplacementInfo_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DeviceReplacementResponse,
    (json),
    DeviceReplacementResponse_Fields)

std::vector<DeviceReplacementInfo>::iterator DeviceReplacementResponse::addSection(const QString& section)
{
    for (auto itr = report.begin(); itr != report.end(); ++itr)
    {
        if (itr->name == section)
            return itr;
    }
    report.push_back({ section });
    return std::prev(report.end());
}

void DeviceReplacementResponse::add(
    const QString& section,
    DeviceReplacementInfo::Level level,
    const QString& message)
{
    auto itr = addSection(section);
    if (level > itr->level)
        itr->messages.clear();
    else if (level < itr->level)
        return;
    itr->level = level;
    if (!message.isNull())
        itr->messages.insert(message);
}

void DeviceReplacementResponse::info(const QString& section, const QString& message)
{
    add(section, DeviceReplacementInfo::Level::info, message);
}

void DeviceReplacementResponse::warning(const QString& section, const QString& message)
{
    add(section, DeviceReplacementInfo::Level::warning, message);
}

void DeviceReplacementResponse::error(const QString& section, const QString& message)
{
    add(section, DeviceReplacementInfo::Level::error, message);
}

void DeviceReplacementResponse::critical(const QString& section, const QString& message)
{
    add(section, DeviceReplacementInfo::Level::critical, message);
}

} // namespace nx::vms::api

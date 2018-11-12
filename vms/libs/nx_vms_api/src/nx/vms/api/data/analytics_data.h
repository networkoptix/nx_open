#pragma once

#include <nx/vms/api/data/resource_data.h>

namespace nx::vms::api {

struct NX_VMS_API AnalyticsPluginData: ResourceData
{
    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;
};

#define AnalyticsPluginData_Fields ResourceData_Fields

struct NX_VMS_API AnalyticsEngineData: ResourceData
{
    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;
};

#define AnalyticsEngineData_Fields ResourceData_Fields

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::AnalyticsPluginData)
Q_DECLARE_METATYPE(nx::vms::api::AnalyticsEngineData)

#pragma once

#include <QtCore/QString>

#include <nx/utils/uuid.h>
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

/**
 * Information about plugin module.
 */
struct NX_VMS_API PluginModuleData: Data
{
    QString name;
    QString description;
    QString libraryName;
    QString vendor;
    QString version;

    // TODO: #vkutin #mschevchenko Add library loaded status, most probably as an enumeration.
};
#define PluginModuleData_Fields (name)(description)(libraryName)(vendor)(version)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::AnalyticsPluginData)
Q_DECLARE_METATYPE(nx::vms::api::AnalyticsEngineData)
Q_DECLARE_METATYPE(nx::vms::api::PluginModuleData)
Q_DECLARE_METATYPE(nx::vms::api::PluginModuleDataList)

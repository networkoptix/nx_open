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
    enum class LoadingType
    {
        normal,
        optional,
    };

    enum class Status
    {
        loadedNormally,
        notLoadedBecauseOfError,
        notLoadedBecauseOfBlackList,
        notLoadedBecausePluginIsOptional,
    };

    QString name;
    QString description;
    QString libraryName;
    QString vendor;
    QString version;
    LoadingType loadingType = LoadingType::normal;
    Status status = Status::loadedNormally;
    QString statusMessage;

    // TODO: #vkutin #mschevchenko Add library loaded status, most probably as an enumeration.
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginModuleData::LoadingType)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginModuleData::Status)
#define PluginModuleData_Fields (name) \
    (description) \
    (libraryName) \
    (vendor) \
    (version) \
    (loadingType) \
    (status) \
    (statusMessage)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::AnalyticsPluginData)
Q_DECLARE_METATYPE(nx::vms::api::AnalyticsEngineData)
Q_DECLARE_METATYPE(nx::vms::api::PluginModuleData)
Q_DECLARE_METATYPE(nx::vms::api::PluginModuleDataList)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginModuleData::LoadingType, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginModuleData::Status, (lexical), NX_VMS_API)

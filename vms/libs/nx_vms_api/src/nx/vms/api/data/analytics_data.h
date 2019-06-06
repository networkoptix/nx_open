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
struct NX_VMS_API PluginInfo: Data
{
    enum class Optionality
    {
        nonOptional,
        optional,
    };

    enum class Status
    {
        loaded,
        notLoadedBecauseOfError,
        notLoadedBecauseOfBlackList,
        notLoadedBecauseOptional,
    };

    enum class Error
    {
        undefined,
        cannotLoadLibrary, //< OS cannot load the library file.
        invalidLibrary, //< The library doesn't seem a valid Nx Plugin library.
        libraryFailure, //< The plugin library failed to initialize.
        unsupportedVersion //< The plugin API version is no longer supported.
    };

    QString name;
    QString description;
    QString libraryName;
    QString vendor;
    QString version;
    Optionality optionality = Optionality::nonOptional;
    Status status = Status::loaded;
    QString statusMessage;
    Error errorCode = Error::undefined;
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Optionality)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Status)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Error)
#define PluginInfo_Fields (name) \
    (description) \
    (libraryName) \
    (vendor) \
    (version) \
    (optionality) \
    (status) \
    (statusMessage) \
    (errorCode)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::AnalyticsPluginData)
Q_DECLARE_METATYPE(nx::vms::api::AnalyticsEngineData)
Q_DECLARE_METATYPE(nx::vms::api::PluginInfo)
Q_DECLARE_METATYPE(nx::vms::api::PluginInfoList)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginInfo::Optionality, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginInfo::Status, (lexical), NX_VMS_API)

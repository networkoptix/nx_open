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
 * Information about a Server plugin library.
 *
 * See Apidoc for /api/pluginInfo for details.
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
        noError,
        cannotLoadLibrary,
        invalidLibrary,
        libraryFailure,
        badManifest,
        unsupportedVersion
    };

    enum class MainInterface
    {
        undefined,
        nxpl_PluginInterface,
        nxpl_Plugin,
        nxpl_Plugin2,
        nx_sdk_IPlugin,
        nx_sdk_analytics_IPlugin,
    };

    QString name;
    QString description;
    QString libraryFilename;
    QString vendor;
    QString version;
    Optionality optionality = Optionality::nonOptional;
    Status status = Status::loaded;
    QString statusMessage;
    Error errorCode = Error::noError;
    MainInterface mainInterface = MainInterface::undefined;
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Optionality)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Status)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Error)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::MainInterface)
#define PluginInfo_Fields (name) \
    (description) \
    (libraryFilename) \
    (vendor) \
    (version) \
    (optionality) \
    (status) \
    (statusMessage) \
    (errorCode) \
    (mainInterface)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::AnalyticsPluginData)
Q_DECLARE_METATYPE(nx::vms::api::AnalyticsEngineData)
Q_DECLARE_METATYPE(nx::vms::api::PluginInfo)
Q_DECLARE_METATYPE(nx::vms::api::PluginInfoList)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginInfo::Optionality, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginInfo::Status, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginInfo::Error, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::PluginInfo::MainInterface, (lexical), NX_VMS_API)

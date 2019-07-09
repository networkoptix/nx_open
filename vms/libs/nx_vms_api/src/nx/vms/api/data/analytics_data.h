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

/**%apidoc Information about a Server plugin library.*/
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
        /**%apidoc
         * No error.
         */
        noError,

        /**%apidoc
         * OS cannot load the library file.
         */
        cannotLoadLibrary,

        /**%apidoc
         * The library does not seem to be a valid Nx Plugin library, e.g. no expected entry point
         * functions found.
         */
        invalidLibrary,

        /**%apidoc
         * The plugin library failed to initialize, e.g. its entry point function returned an
         * error.
         */
        libraryFailure,

        /**%apidoc
         * The plugin has returned a bad manifest, e.g. null, empty, non-json, or json with an
         * unexpected structure.
         */
        badManifest,

        /**%apidoc
         * The plugin API version is no longer supported.
         */
        unsupportedVersion,
    };

    enum class MainInterface
    {
        undefined,
        nxpl_PluginInterface, /**<%apidoc Base interface for the old 3.2 SDK. */
        nxpl_Plugin, /**<%apidoc Old 3.2 SDK plugin supporting roSettings. */
        nxpl_Plugin2, /**<%apidoc Old 3.2 SDK plugin supporting pluginContainer. */
        nx_sdk_IPlugin, /**<%apidoc Base interface for the new 4.0 SDK. */
        nx_sdk_analytics_IPlugin, /**<%apidoc New 4.0 SDK Analytics plugin. */
    };

    /**%apidoc
     * Name of the plugin from its manifest.
     */
    QString name;

    /**%apidoc
     * Description of the plugin from its manifest.
     */
    QString description;

    /**%apidoc
     * Absolute path to the plugin dynamic library.
     */
    QString libraryFilename;

    /**%apidoc Absolute path to the plugin's dedicated directory where its dynamic library resides
     * together with its possible dependencies, or an empty string if the plugin resides in a
     * common directory with other plugins.
     */
    QString homeDir;

    /**%apidoc
     * Vendor of the plugin from its manifest.
     */
    QString vendor;

    /**%apidoc
     * Version of the plugin from its manifest.
     */
    QString version;

    /**%apidoc
     * Whether the plugin resides in "plugins_optional" folder or in the regular "plugins" folder.
     */
    Optionality optionality = Optionality::nonOptional;

    /**%apidoc
     * Status of the plugin after the plugin loading attempt.
     */
    Status status = Status::loaded;

    /**%apidoc
     * Message in English with details about the plugin loading attempt.
     */
    QString statusMessage;

    /**%apidoc
     * If the plugin status is "notLoadedBecauseOfError", describes the error.
     */
    Error errorCode = Error::noError;

    /**%apidoc
     * The highest interface that the Plugin object (the one returned by the plugin entry function)
     * supports via queryInterface().
     */
    MainInterface mainInterface = MainInterface::undefined;
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Optionality)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Status)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::Error)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PluginInfo::MainInterface)
#define PluginInfo_Fields (name) \
    (description) \
    (libraryFilename) \
    (homeDir) \
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

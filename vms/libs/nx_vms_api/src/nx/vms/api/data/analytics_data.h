// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>
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

/**%apidoc
 * Information about a Server Plugin and its dynamic library. If the Plugin object was not created
 * because of issues with the dynamic library, PluginInfo keeps information about these issues. If
 * there is more than one Plugin object created by the same dynamic library, each will have its own
 * instance of PluginInfo.
 */
struct NX_VMS_API PluginInfo
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Optionality,
        nonOptional = 0,
        optional = 1
    )

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status,
        loaded = 0,
        notLoadedBecauseOfError = 1,
        notLoadedBecauseOfBlackList = 2,
        notLoadedBecauseOptional = 3
    )

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Error,
        /**%apidoc
         * No error.
         */
        noError = 0,

        /**%apidoc
         * OS cannot load the library file.
         */
        cannotLoadLibrary = 1,

        /**%apidoc
         * The library does not seem to be a valid Nx Plugin library, e.g. no expected entry point
         * functions found.
         */
        invalidLibrary = 2,

        /**%apidoc
         * The plugin library failed to initialize, e.g. its entry point function returned an
         * error.
         */
        libraryFailure = 3,

        /**%apidoc
         * The plugin has returned a bad manifest, e.g. null, empty, non-json, or json with an
         * unexpected structure.
         */
        badManifest = 4,

        /**%apidoc
         * The plugin API version is no longer supported.
         */
        unsupportedVersion = 5,

        /**%apidoc
         * Some internal error has occurred which made the proper PluginInfo structure unavailable.
         * In this case the error message is stored in statusMessage field.
         */
        internalError = 6
    )

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(MainInterface,
        undefined = 0,
        nxpl_PluginInterface = 1, /**<%apidoc Base interface for the old 3.2 SDK. */
        nxpl_Plugin = 2, /**<%apidoc Old 3.2 SDK plugin supporting roSettings. */
        nxpl_Plugin2 = 3, /**<%apidoc Old 3.2 SDK plugin supporting pluginContainer. */
        nx_sdk_IPlugin = 4, /**<%apidoc Base interface for the new 4.0 SDK. */
        nx_sdk_analytics_IPlugin = 5 /**<%apidoc New 4.0 SDK Analytics plugin. */
    )

    /**%apidoc
     * Name of the plugin from its manifest.
     */
    QString name;

    /**%apidoc
     * Description of the plugin from its manifest.
     */
    QString description;

    /**%apidoc
     * Plugin name for logging: library file name with no `lib` prefix (on Linux) and no extension.
     */
    QString libName;

    /**%apidoc
     * Absolute path to the plugin dynamic library.
     */
    QString libraryFilename;

    /**%apidoc
     * Absolute path to the plugin's dedicated directory where its dynamic library resides together
     * with its possible dependencies, or an empty string if the plugin resides in a common
     * directory with other plugins.
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
     * Whether the plugin resides in "pluins_optional" folder or in the regular "plugins" folder.
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

    /**%apidoc
     * For non-Analytics plugins and for device-independent Analytics plugins, is always set to
     * true. For device-dependent Analytics plugins, is set to true if and only if the plugin has
     * ever had a DeviceAgent since the Server start.
     */
    bool isActive = true;

    /**%apidoc
     * Version of the SDK, as reported by plugin's function nxSdkVersion()). Not empty - if the
     * plugin fails to report the SDK version, this value is set to the description of the reason.
     */
    QString nxSdkVersion;

    /**%apidoc
     * For Plugins created via multi-IPlugin entry point function, a 0-based index of the IPlugin
     * instance corresponding to this PluginInfo instance. Otherwise, -1.
     */
    int instanceIndex = -1;

    /**%apidoc
     * For Plugins created via multi-IPlugin entry point function, an Id of the IPlugin instance
     * corresponding to this PluginInfo instance. Otherwise, empty.
     */
    QString instanceId;
};

#define PluginInfo_Fields (name) \
    (description) \
    (libName) \
    (libraryFilename) \
    (homeDir) \
    (vendor) \
    (version) \
    (optionality) \
    (status) \
    (statusMessage) \
    (errorCode) \
    (mainInterface) \
    (isActive) \
    (nxSdkVersion) \
    (instanceIndex) \
    (instanceId)

/**%apidoc
 * Information about bound resources. For Analytics Plugins contains information about the
 * resources bound to its Engine. For Device Plugins the information is related to the resources
 * bound to the Plugin itself.
 */
struct NX_VMS_API PluginResourceBindingInfo
{
    /**%apidoc
     * Id of the Engine in the case of Analytics Plugins, an empty string for Device Plugins.
     */
    QString id;

    /**%apidoc
     * Name of the Engine (for Analytics Plugins) or the Plugin (for Device Plugins).
     */
    QString name;

    /**%apidoc
     * For Analytics Plugins - number of resources on which the Engine is enabled automatically or
     * by the User. In the case of Device Plugins - the number of resources that are produced by
     * the Plugin.
     */
    int boundResourceCount = 0;

    /**%apidoc
     * For Analytics Plugins - number of resources on which the Engine is enabled automatically or
     * by the User and that are currently online (thus a Device Agent is created for them). In
     * the case of Device Plugins - the number of resources that are produced by the Plugin and are
     * online.
     */
    int onlineBoundResourceCount = 0;
};

#define PluginResourceBindingInfo_Fields \
    (id) \
    (name) \
    (boundResourceCount) \
    (onlineBoundResourceCount)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(PluginResourceBindingInfo)

/**%apidoc Extended information about a Server Plugin. */
struct NX_VMS_API PluginInfoEx: public PluginInfo
{
    PluginInfoEx() = default;
    PluginInfoEx(PluginInfo pluginInfo)
    {
        static_cast<PluginInfo&>(*this) = std::move(pluginInfo);
    }

    PluginInfoEx(const PluginInfoEx& other) = default;
    PluginInfoEx(PluginInfoEx&& other) = default;
    PluginInfoEx& operator=(const PluginInfoEx& other) = default;
    PluginInfoEx& operator=(PluginInfoEx&& other) = default;

    /**%apidoc
     * Array with information about bound resources. For Device Plugins contains zero (if the
     * Plugin is not loaded) or one item. For Analytics Plugins the number of items is equal to the
     * number of Plugin Engines.
     */
    std::vector<PluginResourceBindingInfo> resourceBindingInfo;
};

#define PluginInfoEx_Fields PluginInfo_Fields (resourceBindingInfo)

NX_VMS_API_DECLARE_STRUCT_AND_LIST(PluginInfoEx)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(AnalyticsPluginData)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(AnalyticsEngineData)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(PluginInfo)

using ExtendedPluginInfoByServer = std::map<QnUuid, PluginInfoExList>;

} // namespace nx::vms::api

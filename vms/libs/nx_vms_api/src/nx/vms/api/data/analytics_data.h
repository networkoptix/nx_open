// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/rect_as_string.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/json/value_or_array.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::api {

// TODO: Rename to AnalyticsIntegrationData.
struct NX_VMS_API AnalyticsPluginData: ResourceData
{
    static const QString kResourceTypeName;
    static const nx::Uuid kResourceTypeId;
};
#define AnalyticsPluginData_Fields ResourceData_Fields
NX_VMS_API_DECLARE_STRUCT_AND_LIST(AnalyticsPluginData)
NX_REFLECTION_INSTRUMENT(AnalyticsPluginData, AnalyticsPluginData_Fields)

struct NX_VMS_API AnalyticsEngineData: ResourceData
{
    static const QString kResourceTypeName;
    static const nx::Uuid kResourceTypeId;
};
#define AnalyticsEngineData_Fields ResourceData_Fields
NX_VMS_API_DECLARE_STRUCT_AND_LIST(AnalyticsEngineData)
NX_REFLECTION_INSTRUMENT(AnalyticsEngineData, AnalyticsEngineData_Fields)

/**%apidoc
 * Information about a Server Integration that resides in a plugin (dynamic library). If the
 * Integration object was not created because of issues with the dynamic library, this structure
 * keeps information about these issues. If there is more than one Integration object created by
 * the same dynamic library, each will have its own instance of this structure.
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
        /**%apidoc Base interface for the 4.0+ SDK: nx::sdk::IIntegration or nx::sdk::IPlugin. */
        nx_sdk_IIntegration = 4,
        /**%apidoc New 4.0 SDK: nx::sdk::analytics::Integration or nx::sdk::IPlugin. */
        nx_sdk_analytics_IIntegration = 5
    )

    /**%apidoc
     * Name of the Integration from its manifest.
     */
    QString name;

    /**%apidoc
     * Description of the Integration from its manifest.
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
     * Vendor of the Integration from its manifest.
     */
    QString vendor;

    /**%apidoc
     * Version of the Integration from its manifest.
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
     * If the plugin status is "notLoadedBecauseOfError", describes the error. Can be "internalError"
     * regardless of the status, in which case the statusMessage field contains the error message
     * and the original status message and error code obtained while loading the plugin.
     */
    Error errorCode = Error::noError;

    /**%apidoc
     * The highest interface that the Integration object (the one returned by the plugin entry
     * point function) supports via queryInterface().
     */
    MainInterface mainInterface = MainInterface::undefined;

    /**%apidoc
     * For non-Analytics Integrations and for device-independent Analytics Integrations, is always
     * set to true. For device-dependent Analytics Integrations, is set to true if and only if the
     * Integration has ever had a DeviceAgent since the Server start.
     */
    bool isActive = true;

    /**%apidoc
     * Version of the SDK, as reported by plugin's function nxSdkVersion()). Not empty - if the
     * plugin fails to report the SDK version, this value is set to the description of the reason.
     */
    QString nxSdkVersion;

    /**%apidoc
     * For Plugins created via multi-IIntegration entry point function, a 0-based index of the IIntegration
     * instance corresponding to this PluginInfo instance. Otherwise, -1.
     */
    int instanceIndex = -1;

    /**%apidoc
     * For Plugins created via multi-IIntegration entry point function, an id of the IIntegration
     * instance corresponding to this PluginInfo instance, or, if the id is not available -
     * instanceIndex as a string. Otherwise, empty.
     */
    QString instanceId;
};
#define PluginInfo_Fields \
    (name) \
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
NX_VMS_API_DECLARE_STRUCT_AND_LIST(PluginInfo)
NX_REFLECTION_INSTRUMENT(PluginInfo, PluginInfo_Fields)

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
     * For Analytics Integrations - number of resources on which the Engine is enabled
     * automatically or by the User and that are currently online (thus a Device Agent is created
     * for them). In the case of Device Integrations - the number of Resources that are produced by
     * the Integration and are online.
     */
    int onlineBoundResourceCount = 0;
};
#define PluginResourceBindingInfo_Fields \
    (id) \
    (name) \
    (boundResourceCount) \
    (onlineBoundResourceCount)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(PluginResourceBindingInfo)
NX_REFLECTION_INSTRUMENT(PluginResourceBindingInfo, PluginResourceBindingInfo_Fields)

/**%apidoc Extended information about a Server Plugin. */
struct NX_VMS_API PluginInfoEx: PluginInfo
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
     * Array with information about bound Resources. For Device Integrations contains zero (if the
     * plugin is not loaded) or one item. For Analytics Integrations, the number of items is equal
     * to the number of Engines of the Integration.
     */
    std::vector<PluginResourceBindingInfo> resourceBindingInfo;
};
#define PluginInfoEx_Fields PluginInfo_Fields (resourceBindingInfo)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(PluginInfoEx)
NX_REFLECTION_INSTRUMENT(PluginInfoEx, PluginInfoEx_Fields)

using ExtendedPluginInfoByServer = std::map<nx::Uuid, PluginInfoExList>;

//-------------------------------------------------------------------------------------------------

struct BestShotV4
{
    std::chrono::milliseconds timestampMs;

    /**%apidoc[opt]:string Coordinates in range [0..1]. The format is `{x},{y},{width}x{height}`. */
    std::optional<RectAsString> boundingBox;

    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;

    // TODO: When Best Shot Attributes are implemented on the Server, add them here.
};
#define BestShotV4_Fields \
    (timestampMs)(boundingBox)(streamIndex)
NX_VMS_API_DECLARE_STRUCT_EX(BestShotV4, (json));
NX_REFLECTION_INSTRUMENT(BestShotV4, BestShotV4_Fields)

/**%apidoc
 * Common properties of an image taken from a video frame.
 */
struct ObjectTrackVideoFrameImageInfo
{
    std::chrono::milliseconds timestampMs{};

    /**%apidoc:string
     * Specifies the image rectangle within a frame, coordinates in range [0..1], in the format
     * `{x},{y},{width}x{height}`.
     */
    RectAsString boundingBox;
    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;
};
#define ObjectTrackVideoFrameImageInfo_Fields \
    (timestampMs)(boundingBox)(streamIndex)
NX_VMS_API_DECLARE_STRUCT_EX(ObjectTrackVideoFrameImageInfo, (json));
NX_REFLECTION_INSTRUMENT(ObjectTrackVideoFrameImageInfo, ObjectTrackVideoFrameImageInfo_Fields)

struct ObjectTrackTitle
{
    /**%apidoc[opt] Single-line text to be shown to the user. */
    QString text;

    /**%apidoc
     * Is present if the Title has an image, and this image was taken from a video frame.
     */
    std::optional<ObjectTrackVideoFrameImageInfo> imageInfo;

    /**%apidoc
     * Whether the Title has an image, either taken from a video frame (then imageInfo is present),
     * or received from an Integration in the form of pixel data.
     * `GET /rest/v{4-}/analytics/objectTracks/{id}/titleImage` or
     * `GET /rest/v{4-}/analytics/objectTracks/{id}/titleImage.{format}`.
     */
    bool isImageAvailable = false;

    bool operator<(const ObjectTrackTitle& other) const noexcept { return text < other.text; }
};
#define ObjectTrackTitle_Fields (text)(imageInfo)(isImageAvailable)
NX_VMS_API_DECLARE_STRUCT_EX(ObjectTrackTitle, (json));
NX_REFLECTION_INSTRUMENT(ObjectTrackTitle, ObjectTrackTitle_Fields)

struct BaseObjectRegion
{
    /**%apidoc
     * Roughly defines a region on a video frame where the Object(s) were detected, as an array of
     * bytes encoded in base64.
     * <br/>
     * The region is represented as a grid of 32 rows and 44 columns, stored by columns. The grid
     * coordinate system starts at the top left corner, the row index grows down and the column
     * index grows right. This grid is represented as a contiguous array, each bit of which
     * corresponds to the state of a particular cell of the grid (1 if the region includes the
     * cell, 0 otherwise). The bit index for a cell with coordinates (column, row) can be
     * calculated using the following formula: bitIndex = gridHeight * column + row.
     * <br/>
     * NOTE: This is the same binary format that is used by the VMS for motion detection metadata.
     */
    QByteArray boundingBoxGrid;
};
#define BaseObjectRegion_Fields (boundingBoxGrid)
NX_VMS_API_DECLARE_STRUCT(BaseObjectRegion);
NX_REFLECTION_INSTRUMENT(BaseObjectRegion, BaseObjectRegion_Fields)

/**%apidoc Attribute of an analytics entity. */
struct AnalyticsAttribute
{
    QString name;
    QString value;

    AnalyticsAttribute() = default;
    AnalyticsAttribute(const QString& name, const QString& value): name(name), value(value) {}
};
#define AnalyticsAttribute_Fields (name)(value)
NX_VMS_API_DECLARE_STRUCT(AnalyticsAttribute);
NX_REFLECTION_INSTRUMENT(AnalyticsAttribute, AnalyticsAttribute_Fields)

struct NX_VMS_API ObjectTrackV4
{
    nx::Uuid id;

    /**%apidoc Id of a Device the Object has been detected on. */
    nx::Uuid deviceId;

    /**%apidoc Id of an Object Type of the last received Object Metadata in the Track. */
    QString objectTypeId;

    std::chrono::milliseconds firstAppearanceTimeMs;

    std::chrono::milliseconds lastAppearanceTimeMs;

    BaseObjectRegion objectRegion;

    std::vector<AnalyticsAttribute> attributes;

    std::optional<BestShotV4> bestShot;

    std::optional<ObjectTrackTitle> title = std::nullopt;

    nx::Uuid analyticsEngineId;
};
#define ObjectTrackV4_Fields \
    (id)(deviceId)(objectTypeId)(firstAppearanceTimeMs)(lastAppearanceTimeMs) \
    (objectRegion)(attributes)(bestShot)(title)(analyticsEngineId)
NX_VMS_API_DECLARE_STRUCT_EX(ObjectTrackV4, (json));
NX_REFLECTION_INSTRUMENT(ObjectTrackV4, ObjectTrackV4_Fields)

struct ObjectTrackFilterFreeText
{
    /**%apidoc[opt]
     * Text to match within Object Track Attributes. The text is actually an expression in the
     * special language. Its syntax will likely evolve in the future, and is designed to be close
     * to a "free text" search request in simple cases, so it is described with examples (rather
     * than with a formal definition):
     * <ul>
     * <li>Search for Object Tracks which have an Attribute name or an Attribute value (called here
     *     a string) containing a certain text:
     *     <ul>
     *     <li>`abc`: Match if the word `abc` (case-insensitive) is found at any position in the
     *         string.</li>
     *     <li>`abc def`: Match if both words `abc` and `def` (case-insensitive) are found at any
     *         position in the string, in any order. Any number of words can be specified.</li>
     *     </ul>
     * </li>
     * <li>Search for Object Tracks which have an Attribute with the specified name with a value
     *     containing certain text:
     *     <ul>
     *     <li>`param: expression`: Match if the Attribute with the name `param` is present and its
     *         value matches the expression specified after `:` using the same features as in the
     *         examples above.</li>
     *     <li>`param: "complete"` or `param: $complete^`: Match if the Attribute with the name
     *         `param` is present and its value is exactly `complete` (without any prefix or
     *         suffix).</li>
     *     <li>NOTE: To specify an Attribute name which contains spaces, colons or other special
     *         characters, enquote the Attribute name:
     *         <ul>
     *         <li>`"License Plate": 123`</li>
     *         </ul>
     *     </li>
     *     <li>NOTE: To specify a fragment of an Attribute value which contains quotes, colons,
     *         backslashes or dollar signs, prepend these characters with a backslash:
     *         <ul>
     *         <li>`param: abc\"quoted\"\$\def\\ghi`: Match if the Attribute value contains the
     *             text `abc"quoted"$def\ghi` (case-insensitive) at any position.</li>
     *         </ul>
     *     <li>NOTE: The specified fragment of an Attribute value must contain at least 3
     *         characters.</li>
     *     <li>NOTE: To require that the specified fragment of an Attribute value must start at the
     *          beginning of the value or end at the end of the value, use the symbols `^` or `$`
     *          respectively:
     *          <ul>
     *          <li>`glasses=$red`: Match both `glasses=red` and `glasses=reddish` but not
     *              `glasses=dark-red`.</li>
     *          <li>`glasses=red^`: Match both `glasses=red` and `glasses=dark-red` but not
     *              `glasses=reddish`.</li>
     *          </ul>
     *     </li>
     *     </ul>
     * </li>
     * <li>Search for Object Tracks which have the specified Attribute present, regardless of its
     *     value:
     *     <ul>
     *     <li>`param:` or `$param`: Match if an Attribute with the name `param` is present.</li>
     *     </ul>
     * </li>
     * <li>Search for Object Tracks which have at least one Attribute (i.e. on some particular
     *     video frame in the Object Track) with the specified name with a numeric value that
     *     matches the specified condition:
     *     <ul>
     *     <li>`speed=5`: Match if there is a value equal to 5.</li>
     *     <li>`speed>5`: Match if there is a value greater than 5.</li>
     *     <li>`speed>=5`: Match if there is a value greater than or equal to 5.</li>
     *     <li>`speed<5`: Match if there is a value less than 5.</li>
     *     <li>`speed<=5`: Match if there is a value less than or equal to 5.</li>
     *     <li>`speed=[5...10]` or `speed=5...10`: Match if there is a value in the range from 5 to
     *         10 (inclusive).</li>
     *     <li>`speed=(5...10)`: Match if there is a value which is greater than 5 and less than 10
     *         (i.e. 5 to 10 non-inclusive).</li>
     *     <li>`speed=[-5.4...7.2)`: Match if there is a value which is greater than or equal to
     *         -5.4 and strictly less than 7.2.</li>
     *     </ul>
     * </li>
     * <li>Search for Object Tracks which do not have any Attributes matching the specified
     *     condition:
     *     <ul>
     *     <li>`!wearsGlasses`: Match if the Object Track contains no Attributes with the name
     *         `wearsGlasses`.</li>
     *     </li>`color!=red`: Match if the Object Track does not contain an Attribute with the name
     *         `color` with the value `red`.</li>
     *     </ul>
     * </li>
     * <li>NOTE: If an Object Track contains an Attribute with the name containing a period, e.g.
     *     `glasses.color=red`, the Attribute name specified in the search expression can be a
     *     prefix of this name until the period, e.g. `glasses=red` will match.</li>
     * <li>NOTE: To match any Attribute with the name starting from a certain prefix, append an
     *     asterisk to this prefix: `glas*=red` will match both `glasses.color=red` and
     *     `glassColor=red`.</li>
     * </ul>
     */
    QString freeText;
};
#define ObjectTrackFilterFreeText_Fields (freeText)
NX_VMS_API_DECLARE_STRUCT_EX(ObjectTrackFilterFreeText, (json));
NX_REFLECTION_INSTRUMENT(ObjectTrackFilterFreeText, ObjectTrackFilterFreeText_Fields)

struct ObjectTrackFilter: IdData, ObjectTrackFilterFreeText
{
    /**%apidoc[opt]:stringArray
     * If present, only Object Tracks originating from the specified Device(s) will be considered
     * for search. Device id (can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    json::ValueOrArray<QString> deviceId;

    /**%apidoc[opt]:stringArray
     * If present, only Object Tracks of the specified type(s) will be considered for search.
     */
    json::ValueOrArray<QString> objectTypeId;

    /**%apidoc
     * Start of the time period to search within, in milliseconds since epoch (1970-01-01 00:00, UTC).
     */
    std::optional<std::chrono::milliseconds> startTimeMs;

    /**%apidoc
     * End of the time period to search within, in milliseconds since epoch (1970-01-01 00:00, UTC).
     */
    std::optional<std::chrono::milliseconds> endTimeMs;

    /**%apidoc[opt]:string
     * Coordinates of the picture bounding box to search within; in range [0..1]. The format is
     * `{x},{y},{width}x{height}`.
     */
    std::optional<RectAsString> boundingBox;

    /**%apidoc
     * Maximum number of Object Tracks to return.
     */
    std::optional<int> limit;

    /**%apidoc[opt]:enum
     * Sort order of Object Tracks by a Track start timestamp.
     *     %value asc Ascending order.
     *     %value desc Descending order.
     */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;

    /**%apidoc
     * If specified, only Object Tracks detected by specified engine will be considered for search.
     */
    std::optional<nx::Uuid> analyticsEngineId;
};
#define ObjectTrackFilter_Fields IdData_Fields ObjectTrackFilterFreeText_Fields \
    (deviceId)(objectTypeId)(startTimeMs)(endTimeMs)(boundingBox)(limit) \
    (sortOrder)(analyticsEngineId)
NX_VMS_API_DECLARE_STRUCT_EX(ObjectTrackFilter, (json));
NX_REFLECTION_INSTRUMENT(ObjectTrackFilter, ObjectTrackFilter_Fields)

struct AnalyticsObjectMetadataRequest
{
    /**%apidoc Id of the Analytic Object Track. */
    nx::Uuid id;

    /**%apidoc
     * Flexible Device id (can be obtained from "id", "physicalId" or "logicalId" field via `GET
     * /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     *
     * %// TODO: Remove this parameter by implementing the Device-from-ObjectTrackId logic. The
     *     Device is used for resolution of permissions.
     */
    QString deviceId;
};
#define AnalyticsObjectMetadataRequest_Fields (id)(deviceId)
NX_VMS_API_DECLARE_STRUCT_EX(AnalyticsObjectMetadataRequest, (json));
NX_REFLECTION_INSTRUMENT(AnalyticsObjectMetadataRequest, AnalyticsObjectMetadataRequest_Fields)

struct AnalyticsObjectMetadata
{
    /**%apidoc Id of a Device the Object has been detected on. */
    nx::Uuid deviceId;

    std::chrono::milliseconds timestampMs;
    std::chrono::milliseconds durationMs;

    /**%apidoc:string
     * Coordinates of the bounding box on a video frame where an Object is shown; in range [0..1].
     * The format is: `{x},{y},{width}x{height}`
     */
    RectAsString boundingBox;

    /**%apidoc Attributes of an Object, like a car speed. */
    std::vector<AnalyticsAttribute> attributes;
};
#define AnalyticsObjectMetadata_Fields \
    (deviceId)(timestampMs)(durationMs)(boundingBox)(attributes)
NX_VMS_API_DECLARE_STRUCT_EX(AnalyticsObjectMetadata, (json));
NX_REFLECTION_INSTRUMENT(AnalyticsObjectMetadata, AnalyticsObjectMetadata_Fields)

} // namespace nx::vms::api

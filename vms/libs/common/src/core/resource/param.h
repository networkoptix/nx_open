#pragma once

#include <nx/utils/literal.h>

#include <QString>

/**
 * Contains the keys to treat properties of resources (QnResource and its inheritors)
 * Properties are mutable attributes of a resource.
 * The type of any property is QString.
 * QnResource has a bunch of methods to work with its properties:
 *     hasPropery, getProperty, setProperty, updateProperty, saveProperties, savePropertiesAsync.
 */
namespace ResourcePropertyKey
{

static const QString kAnalog("analog");
static const QString kIsAudioSupported("isAudioSupported");
static const QString kForcedIsAudioSupported("forcedIsAudioSupported");
static const QString kHasDualStreaming("hasDualStreaming");
static const QString kStreamFpsSharing("streamFpsSharing");
static const QString kDts("dts");

/**
 * kMaxFpsThis property key is for compatibility with vms 3.1 only.
 * Do not confuse it with ResourceDataKey::kMaxFps.
 */
static const QString kMaxFps("MaxFPS");

static const QString kMediaCapabilities("mediaCapabilities");
static const QString kMotionWindowCnt("motionWindowCnt");
static const QString kMotionMaskWindowCnt("motionMaskWindowCnt");
static const QString kMotionSensWindowCnt("motionSensWindowCnt");

/**
 * String parameter with following values allowed:
 * - softwaregrid. Software motion calculated on mediaserver.
 * - hardwaregrid. Motion provided by camera.
 * softwaregrid and hardwaregrid can be combined as list split with comma.
 * Empty string means no motion is allowed.
 */
static const QString kSupportedMotion("supportedMotion");

static const QString kTrustCameraTime("trustCameraTime");
static const QString kCredentials("credentials");
static const QString kDefaultCredentials("defaultCredentials");
static const QString kCameraCapabilities("cameraCapabilities");
static const QString kMediaStreams("mediaStreams");
static const QString kBitrateInfos("bitrateInfos");
static const QString kStreamUrls("streamUrls");
static const QString kAudioCodec("audioCodec");
static const QString kUserPreferredPtzPresetType("userPreferredPtzPresetType");
static const QString kDefaultPreferredPtzPresetType("defaultPreferredPtzPresetType");
static const QString kUserIsAllowedToOverridePtzCapabilities(
    "userIsAllowedToOverridePtzCapabilities");
static const QString kPtzCapabilitiesAddedByUser("ptzCapabilitiesAddedByUser");
static const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");
static const QString kCombinedSensorsDescription("combinedSensorsDescription");
static const QString kForcedAudioStream("forcedAudioStream");

// see ResourceDataKey::kOperationalPtzCapabilities
static const QString kPtzCapabilities("ptzCapabilities");

} // namespace ResourcePropertyKey

//-------------------------------------------------------------------------------------------------

/**
 * Contains the keys to get specific data of resources.
 * Specific data is read-only information of a resource that is taken from "resource_data.json".
 * Different specific data pieces have different types.
 * Usually the data is received like this:
 *     QnResourceData resData = commonModule->dataPool()->data(manufacturer, model);
 *     auto maxFps = resourceData.value<float>(ResourceDataKey::kMaxFps, 0.0);
 */
namespace ResourceDataKey
{

static const QString kPossibleDefaultCredentials("possibleDefaultCredentials");
static const QString kMaxFps("MaxFPS");
static const QString kPreferredAuthScheme("preferredAuthScheme");
static const QString kForcedDefaultCredentials("forcedDefaultCredentials");
static const QString kDesiredTransport("desiredTransport");
static const QString kOnvifInputPortAliases("onvifInputPortAliases");
static const QString kOnvifManufacturerReplacement("onvifManufacturerReplacement");
static const QString kTrustToVideoSourceSize("trustToVideoSourceSize");

// Used if we need to control fps via encoding interval (fps when encoding interval is 1).
static const QString kfpsBase("fpsBase");
static const QString kControlFpsViaEncodingInterval("controlFpsViaEncodingInterval");
static const QString kFpsBounds("fpsBounds");
static const QString kUseExistingOnvifProfiles("useExistingOnvifProfiles");
static const QString kForcedSecondaryStreamResolution("forcedSecondaryStreamResolution");
static const QString kDesiredH264Profile("desiredH264Profile");
static const QString kForceSingleStream("forceSingleStream");
static const QString kHighStreamAvailableBitrates("highStreamAvailableBitrates");
static const QString kLowStreamAvailableBitrates("lowStreamAvailableBitrates");
static const QString kHighStreamBitrateBounds("highStreamBitrateBounds");
static const QString kLowStreamBitrateBounds("lowStreamBitrateBounds");
static const QString kUnauthorizedTimeoutSec("unauthorizedTimeoutSec");
static const QString kAdvancedParameterOverloads("advancedParameterOverloads");
static const QString kShouldAppearAsSingleChannel("shouldAppearAsSingleChannel");
static const QString kPreStreamConfigureRequests("preStreamConfigureRequests");
static const QString kDisableMultiThreadDecoding("disableMultiThreadDecoding");
static const QString kConfigureAllStitchedSensors("configureAllStitchedSensors");
static const QString kTwoWayAudio("2WayAudio");

// see ResourcePropertyKey::kPtzCapabilities
static const QString kOperationalPtzCapabilities("operationalPtzCapabilities");
static const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");

} // namespace ResourceDataKey

//-------------------------------------------------------------------------------------------------

/**
 Contains unsorted keys, that should be divided into
 namespaces ResourcePropertyKey and ResourceDataKey and renamed.
 */
namespace Qn
{

//seem to be buggy
static const QString VIDEO_DISABLED_PARAM_NAME = lit("noVideoSupport");
static const QString FORCE_BITRATE_PER_GOP = lit("bitratePerGOP");
//seem to be buggy

static const QString VIDEO_LAYOUT_PARAM_NAME = lit("VideoLayout");
static const QString VIDEO_LAYOUT_PARAM_NAME2 = lit("videoLayout"); //used in resource_data.json
static const QString kAnalyticsDriversParamName = lit("analyticsDrivers"); //< TODO: Rename to supportedAnalyticsEventTypeIds.
static const QString kGroupPlayParamName = lit("groupplay");
static const QString kCanShareLicenseGroup = lit("canShareLicenseGroup");
static const QString kMediaTraits = lit("mediaTraits");
static const QString kDeviceType = lit("deviceType");

//!Contains QnCameraAdvancedParams in ubjson-serialized state
static const QString CAMERA_ADVANCED_PARAMETERS = lit("cameraAdvancedParams");

static const QString FIRMWARE_PARAM_NAME = lit("firmware");
static const QString IO_SETTINGS_PARAM_NAME = lit("ioSettings");
static const QString IO_CONFIG_PARAM_NAME = lit("ioConfigCapability");
static const QString IO_PORT_DISPLAY_NAMES_PARAM_NAME = lit("ioDisplayName");
static const QString IO_OVERLAY_STYLE_PARAM_NAME = lit("ioOverlayStyle");
static const QString FORCE_ONVIF_PARAM_NAME = lit("forceONVIF");
static const QString IGNORE_ONVIF_PARAM_NAME = lit("ignoreONVIF");

static const QString DW_REBRANDED_TO_ISD_MODEL = lit("isdDwCam");
static const QString ONVIF_VENDOR_SUBTYPE = lit("onvifVendorSubtype");
static const QString DO_NOT_ADD_VENDOR_TO_DEVICE_NAME = lit("doNotAddVendorToDeviceName");
static const QString VIDEO_MULTIRESOURCE_CHANNEL_MAPPING_PARAM_NAME = lit("multiresourceVideoChannelMapping");
static const QString NO_RECORDING_PARAMS_PARAM_NAME = lit("noRecordingParams");
static const QString PARSE_ONVIF_NOTIFICATIONS_WITH_HTTP_READER = lit("parseOnvifNotificationsWithHttpReader");
static const QString DISABLE_HEVC_PARAMETER_NAME = lit("disableHevc");
static const QString kCanConfigureRemoteRecording = lit("canConfigureRemoteRecording");

// Mediaserver common info
static const QString kTimezoneUtcOffset = lit("timezoneUtcOffset");

// Mediaserver info for Statistics
static const QString CPU_ARCHITECTURE = lit("cpuArchitecture");
static const QString CPU_MODEL_NAME = lit("cpuModelName");
static const QString PHYSICAL_MEMORY = lit("physicalMemory");
static const QString BETA = lit("beta");
static const QString PUBLIC_IP = lit("publicIp");
static const QString NETWORK_INTERFACES = lit("networkInterfaces");
static const QString FULL_VERSION = lit("fullVersion");
static const QString PRODUCT_NAME_SHORT = lit("productNameShort");
static const QString SYSTEM_RUNTIME = lit("systemRuntime");
static const QString BOOKMARK_COUNT = lit("bookmarkCount");
static const QString UDT_INTERNET_TRFFIC = lit("udtInternetTraffic_bytes");
static const QString HDD_LIST = lit("hddList");
static const QString HDD_LIST_FILE = lit("/tmp/hddlist");

// Storage
static const QString SPACE = lit("space");

// User
static const QString USER_FULL_NAME = lit("fullUserName");

static const QString kResourceDataParamName = "resource_data.json";
static const QString kReloadAllAdvancedParameters(
    "needToReloadAllAdvancedParametersAfterApply");

} // namespace Qn

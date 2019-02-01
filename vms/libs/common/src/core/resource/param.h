#pragma once

#include <nx/utils/literal.h>

#include <QString>

/**
 * Contains the keys to treat properties of resources (QnResource and its inheritors).
 * Properties are mutable attributes of a resource. The type of any property is QString.
 * QnResource has a bunch of methods to work with its properties:
 *     hasPropery, getProperty, setProperty, updateProperty, saveProperties, savePropertiesAsync.
 * Some properties have default read-only values, which can be read with hasDefaultProperty.
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
    static const QString kPtzCapabilities("ptzCapabilities");
    static const QString kPtzTargetId("ptzTargetId");
    static const QString kUserPreferredPtzPresetType("userPreferredPtzPresetType");
    static const QString kDefaultPreferredPtzPresetType("defaultPreferredPtzPresetType");
    static const QString kUserIsAllowedToOverridePtzCapabilities(
        "userIsAllowedToOverridePtzCapabilities");
    static const QString kPtzCapabilitiesAddedByUser("ptzCapabilitiesAddedByUser");
    static const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");
    static const QString kCombinedSensorsDescription("combinedSensorsDescription");
    static const QString kForcedAudioStream("forcedAudioStream");

    // Used as default property only.
    static const QString kGroupPlayParamName("groupplay");
    static const QString kNoRecordingParams("noRecordingParams");

    static const QString kCanConfigureRemoteRecording("canConfigureRemoteRecording");

    // Contains QnCameraAdvancedParams in ubjson-serialized state.
    static const QString kCameraAdvancedParams("cameraAdvancedParams");

    static const QString kFirmware("firmware");
    static const QString kDeviceType("deviceType");
    static const QString kIoConfigCapability("ioConfigCapability");

    // TODO: rename to kIoDisplayNames
    static const QString kIoDisplayName("ioDisplayName");

    static const QString kIoOverlayStyle("ioOverlayStyle");

    // The next two keys are used both as property keys and as data keys, so they are defined both
    // in ResourcePropertyKey and ResourceDataKey namespaces.
    static const QString kBitratePerGOP("bitratePerGOP");
    static const QString kIoSettings("ioSettings");

    static const QString kVideoLayout("VideoLayout");

    namespace Onvif
    {
        static const QString kMediaUrl("MediaUrl");
        static const QString kDeviceUrl("DeviceUrl");
        static const QString kDeviceID("DeviceID");
    }

    namespace Server
    {
        static const QString kTimezoneUtcOffset("timezoneUtcOffset");
        static const QString kCpuArchitecture("cpuArchitecture");
        static const QString kCpuModelName("cpuModelName");
        static const QString kPhysicalMemory("physicalMemory");
        static const QString kProductNameShort("productNameShort");
        static const QString kFullVersion("fullVersion");
        static const QString kBeta("beta");
        static const QString kPublicIp("publicIp");
        static const QString kSystemRuntime("systemRuntime");
        static const QString kNetworkInterfaces("networkInterfaces");
        static const QString kBookmarkCount("bookmarkCount");
        static const QString kUdtInternetTraffic_bytes("udtInternetTraffic_bytes");
        static const QString kHddList("hddList");
    }
}

//-------------------------------------------------------------------------------------------------

/**
 * Contains the keys to get specific data of resources.
 * Specific data is read-only information of a resource that is taken from "resource_data.json".
 * Different specific data pieces have different types.
 * Usually the data is received like this:
 *     QnResourceData resData = commonModule->dataPool()->data(manufacturer, model);
 *     auto maxFps = resourceData.value<float>(ResourceDataKey::kMaxFps, 0.0);
 * Sometimes (seldom) keys are absent in "resource_data.json", but are used in code, this means,
 * that such keys are reserved for future.
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

    static const QString kPtzTargetChannel("ptzTargetChannel");
    static const QString kOperationalPtzCapabilities("operationalPtzCapabilities");
    static const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");

    // Rename to kForceOnvif and kIgnoreOnvif
    static const QString kForceONVIF("forceONVIF");
    static const QString kIgnoreONVIF("ignoreONVIF");

    static const QString kOnvifVendorSubtype("onvifVendorSubtype");

    static const QString kCanShareLicenseGroup("canShareLicenseGroup");

    static const QString kMediaTraits("mediaTraits");

    // TODO: rename to kDwRebrandedToIsd
    static const QString kIsdDwCam("isdDwCam");

    static const QString kDoNotAddVendorToDeviceName("doNotAddVendorToDeviceName");

    static const QString kMultiresourceVideoChannelMapping("multiresourceVideoChannelMapping");

    static const QString kParseOnvifNotificationsWithHttpReader("parseOnvifNotificationsWithHttpReader");
    static const QString kDisableHevc("disableHevc");

    // Rename?
    static const QString kNeedToReloadAllAdvancedParametersAfterApply("needToReloadAllAdvancedParametersAfterApply");

    // Storage
    // Rename?
    static const QString kSpace("space");

    static const QString kNoVideoSupport("noVideoSupport");

    // The next two keys are used both as property keys and as data keys, so they are defined both
    // in ResourcePropertyKey and ResourceDataKey namespaces.
    static const QString kBitratePerGOP("bitratePerGOP");
    static const QString kIoSettings("ioSettings");

    static const QString kVideoLayout("videoLayout");

    static const QString kRepeatIntervalForSendVideoEncoderMS("repeatIntervalForSendVideoEncoderMS");
} // namespace ResourceDataKey

//-------------------------------------------------------------------------------------------------
// These constants should be moved somewhere else.
namespace Qn
{
static const QString USER_FULL_NAME("fullUserName");
static const QString kResourceDataParamName("resource_data.json");
} // namespace Qn

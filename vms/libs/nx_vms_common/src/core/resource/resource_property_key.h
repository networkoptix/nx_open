// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QString>

/**
 * Contains the keys to treat properties of Resources (QnResource and its inheritors). Properties
 * are mutable attributes of a Resource. The type of any property is QString. Properties are stored
 * independently of the Resource, in a separate dictionary.
 *
 * Each Resource has a Resource type which can have default values for some properties.
 */
namespace ResourcePropertyKey {

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
 * A string parameter with the following values allowed:
 * - softwaregrid. Software motion is calculated on the Server.
 * - hardwaregrid. Motion provided by the Camera.
 * softwaregrid and hardwaregrid can be combined into a comma-separated list.
 * The empty string means no motion is allowed.
 */
static const QString kSupportedMotion("supportedMotion");

static const QString kTrustCameraTime("trustCameraTime");
static const QString kKeepCameraTimeSettings("keepCameraTimeSettings");
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

static const QString kPtzCapabilitiesUserIsAllowedToModify("ptzCapabilitiesUserIsAllowedToModify");
static const QString kPtzCapabilitiesAddedByUser("ptzCapabilitiesAddedByUser");
static const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");
static const QString kPtzPanTiltSensitivity("ptzPanTiltSensitivity");
static const QString kForcedAudioStream("forcedAudioStream");

static const QString kAvailableProfiles("availableProfiles");
static const QString kForcedPrimaryProfile("forcedPrimaryProfile");
static const QString kForcedSecondaryProfile("forcedSecondaryProfile");

// Used as Resource-type-only property for some kind of recorders.
static const QString kGroupPlayParamName("groupplay");

// Used as Resource-type-only property.
static const QString kNoRecordingParams("noRecordingParams");

static const QString kCanConfigureRemoteRecording("canConfigureRemoteRecording");

static const QString kFirmware("firmware");
static const QString kDeviceType("deviceType");
static const QString kIoConfigCapability("ioConfigCapability");

// TODO: Rename to kIoDisplayNames.
static const QString kIoDisplayName("ioDisplayName");

static const QString kIoOverlayStyle("ioOverlayStyle");

// The next three keys are used both as property keys and as data keys, so they are defined both
// in ResourcePropertyKey and ResourceDataKey namespaces.
static const QString kBitratePerGOP("bitratePerGOP");
static const QString kUseMedia2ToFetchProfiles("useMedia2ToFetchProfiles");
static const QString kIoSettings("ioSettings");

static const QString kVideoLayout("VideoLayout");

static const QString kMotionStreamKey("motionStream");
static const QString kForcedMotionDetectionKey("forcedMotionDetection");

/**
 * To use or not to use onvif media2 during camera initialization. The valid values are
 * "autoSelect" (by default), "useIfSupported", "neverUse".
 */
static const QString kUseMedia2ToInitializeCamera("useMedia2ToFetchProfiles");

static const QString kForcedLicenseType("forcedLicenseType");

static const QString kTwoWayAudioEnabled("twoWayAudioEnabled");
static const QString kAudioInputDeviceId("audioInputDeviceId");
static const QString kAudioOutputDeviceId("audioOutputDeviceId");

/**
 * Whether primary stream should be recorded on a camera. Empty if recording is allowed, any
 * positive integer value is treated as forbidden.
 */
static const QString kDontRecordPrimaryStreamKey("dontRecordPrimaryStream");

/**
 * Whether the secondary stream should be recorded on a camera. Empty if the recording is allowed;
 * any positive integer value is treated as forbidden.
 */
static const QString kDontRecordSecondaryStreamKey("dontRecordSecondaryStream");

/**
 * Remote archive before this time will not be synchronized.
 */
static const QString kLastSyncronizedRemoteArchiveTimestampMs(
    "lastSyncronizedRemoteArchiveTimestampMs");

/**
 * Whether to download remote (i.e. on-camera) archives.
 */
static const QString kRemoteArchiveSynchronizationDisabled("remoteArchiveSynchronizationDisabled");

/**
 * Whether to download remote (i.e. on-camera) archives.
 */
static const QString kRemoteArchiveSynchronizationEnabledOnce("remoteArchiveSynchronizationEnabledOnce");

/**
 * Whether to download remote (i.e. on-camera) archives.
 */
static const QString kTimeSyncEnabledOnceDueToRemoteArchiveSync("timeSyncEnabledOnceDueToRemoteArchiveSync");

namespace Onvif {

static const QString kMediaUrl = "MediaUrl";
static const QString kDeviceUrl = "DeviceUrl";
static const QString kDeviceID = "DeviceID";

} // namespace Onvif

namespace Server {

static const QString kTimezoneUtcOffset("timezoneUtcOffset");
static const QString kCpuArchitecture("cpuArchitecture");
static const QString kCpuModelName("cpuModelName");
static const QString kPhysicalMemory("physicalMemory");
static const QString kGuidConflictDetected("guidConflictDetected");
// TODO: #rvasilenko Can we change the property text safely?
static const QString kBrand("productNameShort");
static const QString kFullVersion("fullVersion");
static const QString kPublicationType("publicationType");
static const QString kPublicIp("publicIp");
static const QString kSystemRuntime("systemRuntime");
static const QString kNetworkInterfaces("networkInterfaces");
static const QString kBookmarkCount("bookmarkCount");
static const QString kUdtInternetTraffic_bytes("udtInternetTraffic_bytes");
static const QString kHddList("hddList");
static const QString kNvrPoePortPoweringModes("nvrPoePortPoweringModes");
static const QString kCertificate("certificate");
static const QString kUserProvidedCertificate("userProvidedCertificate");
static const QString kWebCamerasDiscoveryEnabled("webCamerasDiscoveryEnabled");
static const QString kMetadataStorageIdKey = "metadataStorageId";

} // namespace Server

static const std::set<QString> kWriteOnlyNames = {
    kCredentials,
    kDefaultCredentials,
};

} // namespace ResourcePropertyKey

//-------------------------------------------------------------------------------------------------

/**
 * Contains the keys to get the specific data of Resources. The specific data is read-only
 * information of a Resource, that is taken from "resource_data.json". Different specific data
 * pieces have different types. Usually the data is received like this:
 * ```
 *     QnResourceData resData = commonModule->resourceDataPool()->data(manufacturer, model);
 *     auto maxFps = resourceData.value<float>(ResourceDataKey::kMaxFps, 0.0);
 * ```
 * Sometimes (seldom) the keys are absent in "resource_data.json", but are used in the code; this
 * means that such keys are reserved for the future.
 */
namespace ResourceDataKey {
static const QString kPossibleDefaultCredentials("possibleDefaultCredentials");
static const QString kMaxFps("MaxFPS");
static const QString kPreferredAuthScheme("preferredAuthScheme");
static const QString kForcedDefaultCredentials("forcedDefaultCredentials");
static const QString kDesiredTransport("desiredTransport");
static const QString kOnvifInputPortAliases("onvifInputPortAliases");
static const QString kOnvifManufacturerReplacement("onvifManufacturerReplacement");
static const QString kTrustToVideoSourceSize("trustToVideoSourceSize");

// Used if we need to control fps via the encoding interval (fps when encoding interval is 1).
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
static const QString kConfigureAllStitchedSensors("configureAllStitchedSensors");
static const QString kTwoWayAudio("2WayAudio");

static const QString kPtzTargetChannel("ptzTargetChannel");
static const QString kOperationalPtzCapabilities("operationalPtzCapabilities");
static const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");

// TODO: Rename to kForceOnvif and kIgnoreOnvif.
static const QString kForceONVIF("forceONVIF");
static const QString kIgnoreONVIF("ignoreONVIF");

static const QString kOnvifVendorSubtype("onvifVendorSubtype");

static const QString kCanShareLicenseGroup("canShareLicenseGroup");

static const QString kMediaTraits("mediaTraits");

// TODO: Rename to kDwRebrandedToIsd.
static const QString kIsdDwCam("isdDwCam");

static const QString kDoNotAddVendorToDeviceName("doNotAddVendorToDeviceName");

static const QString kMultiresourceVideoChannelMapping("multiresourceVideoChannelMapping");

static const QString kParseOnvifNotificationsWithHttpReader(
    "parseOnvifNotificationsWithHttpReader");
static const QString kPullInputEventsAsOdm("pullInputEventsAsOdm");
static const QString kRenewIntervalForPullingAsOdm("renewIntervalForPullingAsOdm");

static const QString kDisableHevc("disableHevc");
static const QString kIgnoreRtcpReports("ignoreRtcpReports");

static const QString DO_UPDATE_PORT_IN_SUBSCRIPTION_ADDRESS = "doUpdatePortInSubscriptionAddress";

static const QString kDoUpdatePortInSubscriptionAddress("doUpdatePortInSubscriptionAddress");

static const QString kUseInvertedActiveStateForOpenIdleState(
    "useInvertedActiveStateForOpenIdleState");

// TODO: Rename?
static const QString kNeedToReloadAllAdvancedParametersAfterApply(
    "needToReloadAllAdvancedParametersAfterApply");

// Storage
// TODO: Rename?
static const QString kSpace("space");

static const QString kNoVideoSupport("noVideoSupport");

// The next three keys are used both as property keys and as data keys, so they are defined both
// in ResourcePropertyKey and ResourceDataKey namespaces.
static const QString kBitratePerGOP("bitratePerGOP");
static const QString kUseMedia2ToFetchProfiles("useMedia2ToFetchProfiles");
static const QString kIoSettings("ioSettings");

static const QString kVideoLayout("videoLayout");

static const QString kRepeatIntervalForSendVideoEncoderMS("repeatIntervalForSendVideoEncoderMS");
static const QString kMulticastIsSupported("multicastIsSupported");
static const QString kOnvifIgnoreMedia2("onvifIgnoreMedia2");

static const QString kFixWrongUri("fixWrongUri");
static const QString kAlternativeSecondStreamSorter("alternativeSecondStreamSorter");

static const QString kOnvifTimeoutSeconds("onvifTimeoutSeconds");

// Add this many seconds to the VMS System time before uploading it to an ONVIF camera.
static const QString kOnvifSetDateTimeOffset("onvifSetDateTimeOffset");

static const QString kAnalogEncoder("analogEncoder");

// Holes in remote archive smaller than this many seconds will be patched up.
static const QString kOnvifRemoteArchiveMinChunkDuration("onvifRemoteArchiveMinChunkDuration");

// Skip this many seconds at the start of remote archive when when downloading it.
static const QString kOnvifRemoteArchiveStartSkipDuration("onvifRemoteArchiveStartSkipDuration");

static const QString kDisableRtspMetadataStream("disableRtspMetadataStream");

// Do not attemp donloading remote archive faster than real-time. This is necessary since some
// cameras cannot cope with that.
static const QString kOnvifRemoteArchiveDisableFastDownload(
    "onvifRemoteArchiveDisableFastDownload");

} // namespace ResourceDataKey

//-------------------------------------------------------------------------------------------------
// TODO: These constants should be moved somewhere else.
namespace Qn {

static const QString USER_FULL_NAME("fullUserName");
static const QString kResourceDataParamName("resource_data.json");

} // namespace Qn

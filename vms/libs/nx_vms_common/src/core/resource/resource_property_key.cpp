// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_property_key.h"

namespace ResourcePropertyKey {

const QString kAnalog("analog");
const QString kIsAudioSupported("isAudioSupported");
const QString kForcedIsAudioSupported("forcedIsAudioSupported");
const QString kHasDualStreaming("hasDualStreaming");
const QString kStreamFpsSharing("streamFpsSharing");
const QString kDts("dts");

const QString kMaxFps("MaxFPS");

const QString kMediaCapabilities("mediaCapabilities");
const QString kMotionWindowCnt("motionWindowCnt");
const QString kMotionMaskWindowCnt("motionMaskWindowCnt");
const QString kMotionSensWindowCnt("motionSensWindowCnt");

const QString kSupportedMotion("supportedMotion");

const QString kTrustCameraTime("trustCameraTime");
const QString kKeepCameraTimeSettings("keepCameraTimeSettings");
const QString kCredentials("credentials");
const QString kDefaultCredentials("defaultCredentials");
const QString kCameraCapabilities("cameraCapabilities");
const QString kMediaStreams("mediaStreams");
const QString kBitrateInfos("bitrateInfos");
const QString kStreamUrls("streamUrls");
const QString kAudioCodec("audioCodec");
const QString kPtzCapabilities("ptzCapabilities");
const QString kPtzTargetId("ptzTargetId");
const QString kUserPreferredPtzPresetType("userPreferredPtzPresetType");
const QString kDefaultPreferredPtzPresetType("defaultPreferredPtzPresetType");
const QString kTemporaryUserFirstLoginTime("temporaryUserFirstLoginTimeS");

const QString kPtzCapabilitiesUserIsAllowedToModify("ptzCapabilitiesUserIsAllowedToModify");
const QString kPtzCapabilitiesAddedByUser("ptzCapabilitiesAddedByUser");
const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");
const QString kPtzPanTiltSensitivity("ptzPanTiltSensitivity");
const QString kForcedAudioStream("forcedAudioStream");

const QString kAvailableProfiles("availableProfiles");
const QString kForcedPrimaryProfile("forcedPrimaryProfile");
const QString kForcedSecondaryProfile("forcedSecondaryProfile");

const QString kGroupPlayParamName("groupplay");
const QString kNoRecordingParams("noRecordingParams");

const QString kCanConfigureRemoteRecording("canConfigureRemoteRecording");

const QString kFirmware("firmware");
const QString kDeviceType("deviceType");
const QString kIoConfigCapability("ioConfigCapability");

const QString kIoDisplayName("ioDisplayName");

const QString kIoOverlayStyle("ioOverlayStyle");

const QString kBitratePerGOP("bitratePerGOP");
const QString kUseMedia2ToFetchProfiles("useMedia2ToFetchProfiles");
const QString kIoSettings("ioSettings");

const QString kVideoLayout("VideoLayout");

const QString kMotionStreamKey("motionStream");
const QString kForcedMotionDetectionKey("forcedMotionDetection");

const QString kUseMedia2ToInitializeCamera("useMedia2ToFetchProfiles");

const QString kForcedLicenseType("forcedLicenseType");

const QString kTwoWayAudioEnabled("twoWayAudioEnabled");
const QString kAudioInputDeviceId("audioInputDeviceId");
const QString kAudioOutputDeviceId("audioOutputDeviceId");
const QString kCameraHotspotsEnabled("cameraHotspotsEnabled");
const QString kCameraHotspotsData("cameraHotspotsData");

const QString kDontRecordPrimaryStreamKey("dontRecordPrimaryStream");
const QString kDontRecordSecondaryStreamKey("dontRecordSecondaryStream");
const QString kDontRecordAudio("dontRecordAudio");

const QString kLastSyncronizedRemoteArchiveTimestampMs("lastSyncronizedRemoteArchiveTimestampMs");
const QString kRemoteArchiveSynchronizationMode("remoteArchiveSynchronizationMode");
const QString kManualRemoteArchiveSynchronizationTriggered(
    "manualRemoteArchiveSynchronizationTriggered");

const QString kTimeSyncEnabledOnceDueToRemoteArchiveSync("timeSyncEnabledOnceDueToRemoteArchiveSync");

const QString kNoVideoSupport("noVideoSupport");

namespace Onvif {

const QString kMediaUrl("MediaUrl");
const QString kDeviceUrl("DeviceUrl");
const QString kDeviceID("DeviceID");

} // namespace ResourcePropertyKey::Onvif

namespace Server {

const QString kCpuArchitecture("cpuArchitecture");
const QString kCpuModelName("cpuModelName");
const QString kFlavor("flavor");
const QString kOsInfo("osInfo");
const QString kPhysicalMemory("physicalMemory");
const QString kGuidConflictDetected("guidConflictDetected");
const QString kBrand("productNameShort");
const QString kFullVersion("fullVersion");
const QString kPublicationType("publicationType");
const QString kPublicIp("publicIp");
const QString kSystemRuntime("systemRuntime");
const QString kNetworkInterfaces("networkInterfaces");
const QString kBookmarkCount("bookmarkCount");
const QString kUdtInternetTraffic_bytes("udtInternetTraffic_bytes");
const QString kHddList("hddList");
const QString kNvrPoePortPoweringModes("nvrPoePortPoweringModes");
const QString kCertificate("certificate");
const QString kUserProvidedCertificate("userProvidedCertificate");
const QString kWebCamerasDiscoveryEnabled("webCamerasDiscoveryEnabled");
const QString kMetadataStorageIdKey("metadataStorageId");
const QString kTimeZoneInformation("timeZoneInformation");

} // namespace ResourcePropertyKey::Server

namespace WebPage {

const QString kSubtypeKey("subtype");

} // namespace WebPage

const std::set<QString> kWriteOnlyNames{
    kCredentials,
    kDefaultCredentials,
};

// Storage
const QString kPersistentStorageStatusFlagsKey("persistentStorageStatusFlags");

} // namespace ResourcePropertyKey

namespace ResourceDataKey {

const QString kPossibleDefaultCredentials("possibleDefaultCredentials");
const QString kMaxFps("MaxFPS");
const QString kPreferredAuthScheme("preferredAuthScheme");
const QString kForcedDefaultCredentials("forcedDefaultCredentials");
const QString kDesiredTransport("desiredTransport");
const QString kOnvifInputPortAliases("onvifInputPortAliases");
const QString kOnvifManufacturerReplacement("onvifManufacturerReplacement");
const QString kTrustToVideoSourceSize("trustToVideoSourceSize");

const QString kfpsBase("fpsBase");
const QString kControlFpsViaEncodingInterval("controlFpsViaEncodingInterval");
const QString kFpsBounds("fpsBounds");
const QString kUseExistingOnvifProfiles("useExistingOnvifProfiles");
const QString kForcedSecondaryStreamResolution("forcedSecondaryStreamResolution");
const QString kDesiredH264Profile("desiredH264Profile");
const QString kForceSingleStream("forceSingleStream");
const QString kHighStreamAvailableBitrates("highStreamAvailableBitrates");
const QString kLowStreamAvailableBitrates("lowStreamAvailableBitrates");
const QString kHighStreamBitrateBounds("highStreamBitrateBounds");
const QString kLowStreamBitrateBounds("lowStreamBitrateBounds");
const QString kUnauthorizedTimeoutSec("unauthorizedTimeoutSec");
const QString kAdvancedParameterOverloads("advancedParameterOverloads");
const QString kShouldAppearAsSingleChannel("shouldAppearAsSingleChannel");
const QString kPreStreamConfigureRequests("preStreamConfigureRequests");
const QString kConfigureAllStitchedSensors("configureAllStitchedSensors");
const QString kTwoWayAudio("2WayAudio");

const QString kPtzTargetChannel("ptzTargetChannel");
const QString kOperationalPtzCapabilities("operationalPtzCapabilities");
const QString kConfigurationalPtzCapabilities("configurationalPtzCapabilities");

const QString kForceONVIF("forceONVIF");
const QString kIgnoreONVIF("ignoreONVIF");

const QString kOnvifVendorSubtype("onvifVendorSubtype");

const QString kCanShareLicenseGroup("canShareLicenseGroup");

const QString kMediaTraits("mediaTraits");

const QString kIsdDwCam("isdDwCam");

const QString kMultiresourceVideoChannelMapping("multiresourceVideoChannelMapping");

const QString kParseOnvifNotificationsWithHttpReader("parseOnvifNotificationsWithHttpReader");
const QString kPullInputEventsAsOdm("pullInputEventsAsOdm");
const QString kRenewIntervalForPullingAsOdm("renewIntervalForPullingAsOdm");

const QString kDisableHevc("disableHevc");
const QString kIgnoreRtcpReports("ignoreRtcpReports");

const QString DO_UPDATE_PORT_IN_SUBSCRIPTION_ADDRESS = "doUpdatePortInSubscriptionAddress";

const QString kDoUpdatePortInSubscriptionAddress("doUpdatePortInSubscriptionAddress");

const QString kUseInvertedActiveStateForOpenIdleState("useInvertedActiveStateForOpenIdleState");

const QString kNeedToReloadAllAdvancedParametersAfterApply("needToReloadAllAdvancedParametersAfterApply");

const QString kSpace("space");

const QString kNoVideoSupport("noVideoSupport");

const QString kAudioDeviceTypes("audioDeviceTypes");

const QString kBitratePerGOP("bitratePerGOP");
const QString kUseMedia2ToFetchProfiles("useMedia2ToFetchProfiles");
const QString kIoSettings("ioSettings");

const QString kVideoLayout("videoLayout");

const QString kRepeatIntervalForSendVideoEncoderMS("repeatIntervalForSendVideoEncoderMS");
const QString kMulticastIsSupported("multicastIsSupported");
const QString kOnvifIgnoreMedia2("onvifIgnoreMedia2");

const QString kFixWrongUri("fixWrongUri");
const QString kAlternativeSecondStreamSorter("alternativeSecondStreamSorter");

const QString kOnvifTimeoutSeconds("onvifTimeoutSeconds");

const QString kOnvifSetDateTimeOffset("onvifSetDateTimeOffset");

const QString kAnalogEncoder("analogEncoder");

const QString kOnvifRemoteArchiveMinChunkDuration("onvifRemoteArchiveMinChunkDuration");

const QString kOnvifRemoteArchiveStartSkipDuration("onvifRemoteArchiveStartSkipDuration");

const QString kDisableRtspMetadataStream("disableRtspMetadataStream");

const QString kOnvifRemoteArchiveDisableFastDownload("onvifRemoteArchiveDisableFastDownload");

const QString kOnvifRemoteArchiveEnabled("onvifRemoteArchiveEnabled");

const QString kGsoapAdditionalFlags("gsoapAdditionalFlags");

const QString kOnvifIgnoreOutdatedNotifications("onvifIgnoreOutdatedNotifications");

} // namespace ResourceDataKey

//-------------------------------------------------------------------------------------------------
// These constants should be moved somewhere else.
namespace Qn
{

const QString USER_FULL_NAME("fullUserName");
const QString kResourceDataParamName("resource_data.json");

} // namespace Qn

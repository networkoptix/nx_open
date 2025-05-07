// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_property_key.h"

namespace ResourcePropertyKey {
namespace Onvif {

const QString kMediaUrl("MediaUrl");
const QString kDeviceUrl("DeviceUrl");
const QString kDeviceID("DeviceID");

} // namespace ResourcePropertyKey::Onvif

namespace WebPage {

const QString kSubtypeKey("subtype");

} // namespace WebPage

const std::set<QString> kWriteOnlyNames{
    nx::vms::api::device_properties::kCredentials,
    nx::vms::api::device_properties::kDefaultCredentials,
};

// Storage
const QString kPersistentStorageStatusFlagsKey("persistentStorageStatusFlags");
const QString kStorageArchiveMode("storageArchiveMode");
const QString kCloudStorageCapabilities("cloudStorageCapabilities");

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
const QString kFixWrongUriScheme("fixWrongUriScheme");
const QString kAlternativeSecondStreamSorter("alternativeSecondStreamSorter");

const QString kOnvifTimeoutSeconds("onvifTimeoutSeconds");

const QString kOnvifSetDateTimeOffset("onvifSetDateTimeOffset");

const QString kAnalogEncoder("analogEncoder");

const QString kOnvifRemoteArchiveMinChunkDuration("onvifRemoteArchiveMinChunkDuration");

const QString kOnvifRemoteArchiveStartSkipDuration("onvifRemoteArchiveStartSkipDuration");

const QString kDisableRtspMetadataStream("disableRtspMetadataStream");

const QString kOnvifRemoteArchiveDisableFastDownload("onvifRemoteArchiveDisableFastDownload");

const QString kOnvifRemoteArchiveDisabled("onvifRemoteArchiveDisabled");

const QString kOnvifConfigurationParameters("onvifConfigurationParameters");

const QString kTimezoneConfiguration("timezoneConfiguration");

const QString kOnvifIgnoreOutdatedNotifications("onvifIgnoreOutdatedNotifications");

const QString kReRequestOnvifRecordingEventsIfAllEventsHaveSameTime(
    "reRequestOnvifRecordingEventsIfAllEventsHaveSameTime");

const QString kPreferNativeApiForRemoteArchiveSynchronization(
    "preferNativeApiForRemoteArchiveSynchronization");

const QString kOnvifForcePtzGetCompatibleConfigurationsSupported(
    "onvifForcePtzGetCompatibleConfigurationsSupported");

} // namespace ResourceDataKey

//-------------------------------------------------------------------------------------------------
// These constants should be moved somewhere else.
namespace Qn
{

const QString kResourceDataParamName("resource_data.json");

} // namespace Qn

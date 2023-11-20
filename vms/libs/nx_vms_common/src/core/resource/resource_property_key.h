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
NX_VMS_COMMON_API extern const QString kAnalog;
NX_VMS_COMMON_API extern const QString kIsAudioSupported;
NX_VMS_COMMON_API extern const QString kForcedIsAudioSupported;
NX_VMS_COMMON_API extern const QString kHasDualStreaming;
NX_VMS_COMMON_API extern const QString kStreamFpsSharing;
NX_VMS_COMMON_API extern const QString kDts;

/**
 * kMaxFpsThis property key is for compatibility with vms 3.1 only.
 * Do not confuse it with ResourceDataKey::kMaxFps.
 */
NX_VMS_COMMON_API extern const QString kMaxFps;

NX_VMS_COMMON_API extern const QString kMediaCapabilities;
NX_VMS_COMMON_API extern const QString kMotionWindowCnt;
NX_VMS_COMMON_API extern const QString kMotionMaskWindowCnt;
NX_VMS_COMMON_API extern const QString kMotionSensWindowCnt;

/**
 * A string parameter with the following values allowed:
 * - softwaregrid. Software motion is calculated on the Server.
 * - hardwaregrid. Motion provided by the Camera.
 * softwaregrid and hardwaregrid can be combined into a comma-separated list.
 * The empty string means no motion is allowed.
 */
NX_VMS_COMMON_API extern const QString kSupportedMotion;

NX_VMS_COMMON_API extern const QString kTrustCameraTime;
NX_VMS_COMMON_API extern const QString kKeepCameraTimeSettings;
NX_VMS_COMMON_API extern const QString kCredentials;
NX_VMS_COMMON_API extern const QString kDefaultCredentials;
NX_VMS_COMMON_API extern const QString kCameraCapabilities;
NX_VMS_COMMON_API extern const QString kMediaStreams;
NX_VMS_COMMON_API extern const QString kBitrateInfos;
NX_VMS_COMMON_API extern const QString kStreamUrls;
NX_VMS_COMMON_API extern const QString kAudioCodec;
NX_VMS_COMMON_API extern const QString kPtzCapabilities;
NX_VMS_COMMON_API extern const QString kPtzTargetId;
NX_VMS_COMMON_API extern const QString kUserPreferredPtzPresetType;
NX_VMS_COMMON_API extern const QString kDefaultPreferredPtzPresetType;
NX_VMS_COMMON_API extern const QString kTemporaryUserFirstLoginTime;

NX_VMS_COMMON_API extern const QString kPtzCapabilitiesUserIsAllowedToModify;
NX_VMS_COMMON_API extern const QString kPtzCapabilitiesAddedByUser;
NX_VMS_COMMON_API extern const QString kConfigurationalPtzCapabilities;
NX_VMS_COMMON_API extern const QString kPtzPanTiltSensitivity;
NX_VMS_COMMON_API extern const QString kForcedAudioStream;

NX_VMS_COMMON_API extern  const QString kAvailableProfiles;
NX_VMS_COMMON_API extern  const QString kForcedPrimaryProfile;
NX_VMS_COMMON_API extern  const QString kForcedSecondaryProfile;

// Used as Resource-type-only property for some kind of recorders.
NX_VMS_COMMON_API extern const QString kGroupPlayParamName;

// Used as Resource-type-only property.
NX_VMS_COMMON_API extern const QString kNoRecordingParams;

NX_VMS_COMMON_API extern const QString kCanConfigureRemoteRecording;

NX_VMS_COMMON_API extern const QString kFirmware;
NX_VMS_COMMON_API extern const QString kDeviceType;
NX_VMS_COMMON_API extern const QString kIoConfigCapability;

// TODO: Rename to kIoDisplayNames.
NX_VMS_COMMON_API extern const QString kIoDisplayName;

NX_VMS_COMMON_API extern const QString kIoOverlayStyle;

// The next three keys are used both as property keys and as data keys, so they are defined both
// in ResourcePropertyKey and ResourceDataKey namespaces.
NX_VMS_COMMON_API extern const QString kBitratePerGOP;
NX_VMS_COMMON_API extern const QString kUseMedia2ToFetchProfiles;
NX_VMS_COMMON_API extern const QString kIoSettings;

NX_VMS_COMMON_API extern const QString kVideoLayout;

NX_VMS_COMMON_API extern const QString kMotionStreamKey;
NX_VMS_COMMON_API extern const QString kForcedMotionDetectionKey;

/**
 * To use or not to use onvif media2 during camera initialization. The valid values are
 * "autoSelect" (by default), "useIfSupported", "neverUse".
 */
NX_VMS_COMMON_API extern const QString kUseMedia2ToInitializeCamera;

NX_VMS_COMMON_API extern const QString kForcedLicenseType;

NX_VMS_COMMON_API extern const QString kTwoWayAudioEnabled;
NX_VMS_COMMON_API extern const QString kAudioInputDeviceId;
NX_VMS_COMMON_API extern const QString kAudioOutputDeviceId;
NX_VMS_COMMON_API extern const QString kCameraHotspotsEnabled;
NX_VMS_COMMON_API extern const QString kCameraHotspotsData;

/**
 * Whether primary stream should be recorded on a camera. Empty if recording is allowed, any
 * positive integer value is treated as forbidden.
 */
NX_VMS_COMMON_API extern const QString kDontRecordPrimaryStreamKey;

/**
 * Whether the secondary stream should be recorded on a camera. Empty if the recording is allowed;
 * any positive integer value is treated as forbidden.
 */
NX_VMS_COMMON_API extern const QString kDontRecordSecondaryStreamKey;

/**
 * Remote archive before this time will not be synchronized.
 */
NX_VMS_COMMON_API extern const QString kLastSyncronizedRemoteArchiveTimestampMs;

/**
 * Whether to download remote (i.e. on-camera) archives.
 */
NX_VMS_COMMON_API extern const QString kRemoteArchiveSynchronizationMode;

/**
 * Whether manual remote archive import is triggered.
 */
NX_VMS_COMMON_API extern const QString kManualRemoteArchiveSynchronizationTriggered;

/**
 * Whether time sync has been enabled once due to activation of remote archive sync.
 */
NX_VMS_COMMON_API extern const QString kTimeSyncEnabledOnceDueToRemoteArchiveSync;

NX_VMS_COMMON_API extern const QString kNoVideoSupport;

namespace Onvif {

NX_VMS_COMMON_API extern const QString kMediaUrl;
NX_VMS_COMMON_API extern const QString kDeviceUrl;
NX_VMS_COMMON_API extern const QString kDeviceID;
} // namespace Onvif

namespace Server {

NX_VMS_COMMON_API extern const QString kCpuArchitecture;
NX_VMS_COMMON_API extern const QString kCpuModelName;
NX_VMS_COMMON_API extern const QString kFlavor;
NX_VMS_COMMON_API extern const QString kOsInfo;
NX_VMS_COMMON_API extern const QString kPhysicalMemory;
NX_VMS_COMMON_API extern const QString kGuidConflictDetected;
// TODO: #rvasilenko Can we change the property text safely?
NX_VMS_COMMON_API extern const QString kBrand;
NX_VMS_COMMON_API extern const QString kFullVersion;
NX_VMS_COMMON_API extern const QString kPublicationType;
NX_VMS_COMMON_API extern const QString kPublicIp;
NX_VMS_COMMON_API extern const QString kSystemRuntime;
NX_VMS_COMMON_API extern const QString kNetworkInterfaces;
NX_VMS_COMMON_API extern const QString kBookmarkCount;
NX_VMS_COMMON_API extern const QString kUdtInternetTraffic_bytes;
NX_VMS_COMMON_API extern const QString kHddList;
NX_VMS_COMMON_API extern const QString kNvrPoePortPoweringModes;
NX_VMS_COMMON_API extern const QString kCertificate;
NX_VMS_COMMON_API extern const QString kUserProvidedCertificate;
NX_VMS_COMMON_API extern const QString kWebCamerasDiscoveryEnabled;
NX_VMS_COMMON_API extern const QString kMetadataStorageIdKey;

} // namespace Server

NX_VMS_COMMON_API extern const std::set<QString> kWriteOnlyNames;

// Storage
NX_VMS_COMMON_API extern const QString kPersistentStorageStatusFlagsKey;

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
NX_VMS_COMMON_API extern const QString kPossibleDefaultCredentials;
NX_VMS_COMMON_API extern const QString kMaxFps;
NX_VMS_COMMON_API extern const QString kPreferredAuthScheme;
NX_VMS_COMMON_API extern const QString kForcedDefaultCredentials;
NX_VMS_COMMON_API extern const QString kDesiredTransport;
NX_VMS_COMMON_API extern const QString kOnvifInputPortAliases;
NX_VMS_COMMON_API extern const QString kOnvifManufacturerReplacement;
NX_VMS_COMMON_API extern const QString kTrustToVideoSourceSize;

// Used if we need to control fps via the encoding interval (fps when encoding interval is 1).
NX_VMS_COMMON_API extern const QString kfpsBase;
NX_VMS_COMMON_API extern const QString kControlFpsViaEncodingInterval;
NX_VMS_COMMON_API extern const QString kFpsBounds;
NX_VMS_COMMON_API extern const QString kUseExistingOnvifProfiles;
NX_VMS_COMMON_API extern const QString kForcedSecondaryStreamResolution;
NX_VMS_COMMON_API extern const QString kDesiredH264Profile;
NX_VMS_COMMON_API extern const QString kForceSingleStream;
NX_VMS_COMMON_API extern const QString kHighStreamAvailableBitrates;
NX_VMS_COMMON_API extern const QString kLowStreamAvailableBitrates;
NX_VMS_COMMON_API extern const QString kHighStreamBitrateBounds;
NX_VMS_COMMON_API extern const QString kLowStreamBitrateBounds;
NX_VMS_COMMON_API extern const QString kUnauthorizedTimeoutSec;
NX_VMS_COMMON_API extern const QString kAdvancedParameterOverloads;
NX_VMS_COMMON_API extern const QString kShouldAppearAsSingleChannel;
NX_VMS_COMMON_API extern const QString kPreStreamConfigureRequests;
NX_VMS_COMMON_API extern const QString kConfigureAllStitchedSensors;
NX_VMS_COMMON_API extern const QString kTwoWayAudio;

NX_VMS_COMMON_API extern const QString kPtzTargetChannel;
NX_VMS_COMMON_API extern const QString kOperationalPtzCapabilities;
NX_VMS_COMMON_API extern const QString kConfigurationalPtzCapabilities;

// TODO: Rename to kForceOnvif and kIgnoreOnvif.
NX_VMS_COMMON_API extern const QString kForceONVIF;
NX_VMS_COMMON_API extern const QString kIgnoreONVIF;
NX_VMS_COMMON_API extern const QString kAudioDeviceTypes;

NX_VMS_COMMON_API extern const QString kOnvifVendorSubtype;

NX_VMS_COMMON_API extern const QString kCanShareLicenseGroup;

NX_VMS_COMMON_API extern const QString kMediaTraits;

// TODO: Rename to kDwRebrandedToIsd.
NX_VMS_COMMON_API extern const QString kIsdDwCam;

NX_VMS_COMMON_API extern const QString kMultiresourceVideoChannelMapping;

NX_VMS_COMMON_API extern const QString kParseOnvifNotificationsWithHttpReader;
NX_VMS_COMMON_API extern const QString kPullInputEventsAsOdm;
NX_VMS_COMMON_API extern const QString kRenewIntervalForPullingAsOdm;

NX_VMS_COMMON_API extern const QString kDisableHevc;
NX_VMS_COMMON_API extern const QString kIgnoreRtcpReports;

NX_VMS_COMMON_API extern const QString DO_UPDATE_PORT_IN_SUBSCRIPTION_ADDRESS;

NX_VMS_COMMON_API extern const QString kDoUpdatePortInSubscriptionAddress;

NX_VMS_COMMON_API extern const QString kUseInvertedActiveStateForOpenIdleState;

// TODO: Rename?
NX_VMS_COMMON_API extern const QString kNeedToReloadAllAdvancedParametersAfterApply;

NX_VMS_COMMON_API extern const QString kNoVideoSupport;

// The next three keys are used both as property keys and as data keys, so they are defined both
// in ResourcePropertyKey and ResourceDataKey namespaces.
NX_VMS_COMMON_API extern const QString kBitratePerGOP;
NX_VMS_COMMON_API extern const QString kUseMedia2ToFetchProfiles;
NX_VMS_COMMON_API extern const QString kIoSettings;

NX_VMS_COMMON_API extern const QString kVideoLayout;

NX_VMS_COMMON_API extern const QString kRepeatIntervalForSendVideoEncoderMS;
NX_VMS_COMMON_API extern const QString kMulticastIsSupported;
NX_VMS_COMMON_API extern const QString kOnvifIgnoreMedia2;

NX_VMS_COMMON_API extern const QString kFixWrongUri;
NX_VMS_COMMON_API extern const QString kAlternativeSecondStreamSorter;

NX_VMS_COMMON_API extern const QString kOnvifTimeoutSeconds;

// Add this many seconds to the VMS System time before uploading it to an ONVIF camera.
NX_VMS_COMMON_API extern const QString kOnvifSetDateTimeOffset;

NX_VMS_COMMON_API extern const QString kAnalogEncoder;

// Holes in remote archive smaller than this many seconds will be patched up.
NX_VMS_COMMON_API extern const QString kOnvifRemoteArchiveMinChunkDuration;

// Skip this many seconds at the start of remote archive when downloading it.
NX_VMS_COMMON_API extern const QString kOnvifRemoteArchiveStartSkipDuration;

NX_VMS_COMMON_API extern const QString kDisableRtspMetadataStream;

// Do not attempt downloading remote archive faster than real-time. This is necessary since some
// cameras cannot cope with that.
NX_VMS_COMMON_API extern const QString kOnvifRemoteArchiveDisableFastDownload;

// Additional flags that can be passed to gSOAP library.
NX_VMS_COMMON_API extern const QString kGsoapAdditionalFlags;

// Force disable ONVIF PullPoint Notification timestamp validation.
NX_VMS_COMMON_API extern const QString kOnvifIgnoreOutdatedNotifications;

} // namespace ResourceDataKey

//-------------------------------------------------------------------------------------------------
// TODO: These constants should be moved somewhere else.
namespace Qn {

NX_VMS_COMMON_API extern const QString USER_FULL_NAME;
NX_VMS_COMMON_API extern const QString kResourceDataParamName;

} // namespace Qn

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::api {

namespace server_properties {

NX_VMS_API extern const QString kCpuArchitecture;
NX_VMS_API extern const QString kCpuModelName;
NX_VMS_API extern const QString kFlavor;
// Deprecated unused property, was replaced by MediaServerData field.
NX_VMS_API extern const QString kOsInfo;
NX_VMS_API extern const QString kPhysicalMemory;
NX_VMS_API extern const QString kGuidConflictDetected;
NX_VMS_API extern const QString kDeploymentCode;
// TODO: #rvasilenko Can we change the property text safely?
NX_VMS_API extern const QString kBrand;
NX_VMS_API extern const QString kFullVersion;
NX_VMS_API extern const QString kPublicationType;
NX_VMS_API extern const QString kPublicIp;
NX_VMS_API extern const QString kSystemRuntime;
NX_VMS_API extern const QString kNetworkInterfaces;
NX_VMS_API extern const QString kUdtInternetTraffic_bytes;
NX_VMS_API extern const QString kHddList;
NX_VMS_API extern const QString kNvrPoePortPoweringModes;
NX_VMS_API extern const QString kCertificate;
NX_VMS_API extern const QString kUserProvidedCertificate;
NX_VMS_API extern const QString kWebCamerasDiscoveryEnabled;
NX_VMS_API extern const QString kHardwareDecodingEnabled;
NX_VMS_API extern const QString kMetadataStorageIdKey;
NX_VMS_API extern const QString kTimeZoneInformation;
NX_VMS_API extern const QString kPortForwardingConfigurations;

namespace detail {

// To use safely in different DLL to define global QStrings.
inline constexpr char kAnalyticsTaxonomyDescriptorsValue[] = "analyticsTaxonomyDescriptors";

} // namespace detail

// Must not be present in response for rest/v4.
inline const QString kAnalyticsTaxonomyDescriptors = detail::kAnalyticsTaxonomyDescriptorsValue;

} // namespace server_properties

namespace user_properties {

// Used in Desktop, internally.
inline const QString kTemporaryUserFirstLoginTime = "temporaryUserFirstLoginTimeS";

NX_VMS_API extern const QString kUserSettings;

} // namespace user_properties

namespace device_properties {

////////////////////////////////////////////////////////////////////////////////
/// Properties mapped to DeviceModel fields
////////////////////////////////////////////////////////////////////////////////

inline const QString kAnalog = "analog";
inline const QString kCameraCapabilities= "cameraCapabilities";
inline const QString kCompatibleAnalyticsEnginesProperty = "compatibleAnalyticsEngines";
inline const QString kConfigurationalPtzCapabilities = "configurationalPtzCapabilities";
inline const QString kCredentials = "credentials";
inline const QString kDefaultCredentials = "defaultCredentials";
inline const QString kDefaultPreferredPtzPresetType = "defaultPreferredPtzPresetType";
inline const QString kMediaCapabilities = "mediaCapabilities";
inline const QString kMediaStreams = "mediaStreams";
inline const QString kPtzCapabilities = "ptzCapabilities";
inline const QString kPtzCapabilitiesAddedByUser = "ptzCapabilitiesAddedByUser";
inline const QString kPtzCapabilitiesUserIsAllowedToModify = "ptzCapabilitiesUserIsAllowedToModify";
inline const QString kPtzPanTiltSensitivity = "ptzPanTiltSensitivity";
inline const QString kStreamUrls = "streamUrls";
inline const QString kUserEnabledAnalyticsEnginesProperty = "userEnabledAnalyticsEngines";
inline const QString kUserPreferredPtzPresetType = "userPreferredPtzPresetType";

/// Used in Desktop, but from `QnResourcePtr`.
/// Used via `QnVirtualCameraResource`
inline const QString kAudioOutputDeviceId = "audioOutputDeviceId";

/// Used in Desktop, via `Camera`
inline const QString kBitrateInfos = "bitrateInfos";

/// Used in Desktop, via `QnVirtualCameraResource`, internally
inline const QString kBitratePerGOP = "bitratePerGOP";

/// Used in Desktop, but from `QnResourcePtr`.
/// Used via `QnVirtualCameraResource`, internally.
inline const QString kCameraHotspotsEnabled = "cameraHotspotsEnabled";

/// Used in Desktop, via `QnVirtualCameraResource`
/**
 * Whether the secondary stream should be recorded on a camera. Empty if the recording is allowed;
 * any positive integer value is treated as forbidden.
 */
inline const QString kDontRecordSecondaryStreamKey = "dontRecordSecondaryStream";

/// Used in Desktop, but from `QnResourcePtr`.
/// Used via `QnVirtualCameraResource`, internally.
inline const QString kDts = "dts";

/// Used in Desktop, but from `QnResourcePtr`.
/// Used via `QnVirtualCameraResource`, internally
inline const QString kForcedMotionDetectionKey = "forcedMotionDetection";

/// Used in Desktop, internally
inline const QString kHasRtspSettings = "hasRtspSettings";

/// Used in Desktop, via `QnVirtualCameraResource`, internally
inline const QString kIoConfigCapability = "ioConfigCapability";

/// Used in Desktop
inline const QString kIoOverlayStyle = "ioOverlayStyle";

/// Used in Desktop, internally
inline const QString kIoSettings = "ioSettings";

/// Used via `QnVirtualCameraResource`
inline const QString kMediaPort = "mediaPort";

/// Used in Desktop, via `QnVirtualCameraResource`, internally
inline const QString kMotionStreamKey = "motionStream";

////////////////////////////////////////////////////////////////////////////////
/// Internal properties (not to be mapped)
////////////////////////////////////////////////////////////////////////////////
// TODO: #skolesnik Move here unmapped properties that are used ONLY internally (not used in
// Desktop even via `QnVirtualCameraResource` method)

/// Only used internally in QnLiveStreamProvider
/// String value is duplicated as `kAudioCodecParamName( "audioCodec" )`
inline const QString kAudioCodec = "audioCodec";

////////////////////////////////////////////////////////////////////////////////
/// Unmapped Properties
////////////////////////////////////////////////////////////////////////////////

// 'Only used internally' means a key is not used outside server code (for example, by the Desktop
// client) to fetch the property from a REST API response's 'parameters' map.
// If a key is only used as part of `QnVirtualCameraResource` method, it has to be investigated if
// that method is used in Desktop code.

// TODO: #skolesnik Identify which properties used in Cloud

/// Used via `QnVirtualCameraResource`, internally
inline const QString kAudioInputDeviceId = "audioInputDeviceId";

/// Only used via `QnVirtualCameraResource`
inline const QString kAvailableProfiles = "availableProfiles";

/// Used via `QnVirtualCameraResource`, internally
inline const QString kCameraHotspotsData = "cameraHotspotsData";

/// Only used via `QnVirtualCameraResource`
inline const QString kCameraSerialNumber = "serialNumber";

/// Only used via `QnVirtualCameraResource`
inline const QString kCanConfigureRemoteRecording = "canConfigureRemoteRecording";

/// This key already has a mapping to `DeviceModelV1Base`, but it is not directly fetched/removed from `parameters`
inline const QString kDeviceType = "deviceType";

/// Used via `QnVirtualCameraResource`, internally in QnRecordingManager.
/**
 * Whether to record audio track for the primary and secondary streams.
 */
inline const QString kDontRecordAudio = "dontRecordAudio"; //< Seem to be not used anywhere

/// Used via `QnVirtualCameraResource`, internally in QnRecordingManager.
/// Also in `QnVirtualCameraResource::isPrimaryStreamRecorded`
/**
 * Whether primary stream should be recorded on a camera. Empty if recording is allowed, any
 * positive integer value is treated as forbidden.
 */
inline const QString kDontRecordPrimaryStreamKey = "dontRecordPrimaryStream";

/// Used via `QnVirtualCameraResource`, internally
inline const QString kFirmware = "firmware";

/// Used via `QnVirtualCameraResource`, internally
inline const QString kForcedAudioStream = "forcedAudioStream";

/// Only used via `QnVirtualCameraResource`
inline const QString kForcedIsAudioSupported = "forcedIsAudioSupported";

/// Only used via `QnVirtualCameraResource` and `Camera`, internally
inline const QString kForcedLicenseType = "forcedLicenseType";

/// Used via `QnVirtualCameraResource` and in `onvif_stream_reader.cpp`
inline const QString kForcedPrimaryProfile = "forcedPrimaryProfile";

/// Used via `QnVirtualCameraResource` and in `onvif_stream_reader.cpp`
inline const QString kForcedSecondaryProfile = "forcedSecondaryProfile";

/// Only used via `QnVirtualCameraResource`
// Used as Resource-type-only property for some kind of recorders.
inline const QString kGroupPlayParamName = "groupplay";

// TODO: Rename to kIoDisplayNames.
/// Only used internally in `axis_resource.cpp`
inline const QString kIoDisplayName = "ioDisplayName";

/// Used via `QnVirtualCameraResource`, internally and in `onvif_resource.cpp`
inline const QString kIsAudioSupported = "isAudioSupported";

/// Used via `QnVirtualCameraResource`, internally
inline const QString kKeepCameraTimeSettings = "keepCameraTimeSettings";

/// Used via `QnVirtualCameraResource` and in `remote_archive_synchronizer.cpp`
/**
 * Whether manual remote archive import is triggered.
 */
inline const QString kManualRemoteArchiveSynchronizationTriggered =
    "manualRemoteArchiveSynchronizationTriggered";

/// Used via `QnVirtualCameraResource` and internally
/**
 * kMaxFpsThis property key is for compatibility with vms 3.1 only.
 * Do not confuse it with ResourceDataKey::kMaxFps.
 */
inline const QString kMaxFps = "MaxFPS";

/// Used via `QnVirtualCameraResource` and internally
inline const QString kMotionMaskWindowCnt = "motionMaskWindowCnt";

/// Used via `QnVirtualCameraResource` and internally
inline const QString kMotionSensWindowCnt = "motionSensWindowCnt";

/// Used via `QnVirtualCameraResource` and internally
inline const QString kMotionWindowCnt = "motionWindowCnt";

// Used as Resource-type-only property.
/// Only used via `QnVirtualCameraResource`
inline const QString kNoRecordingParams = "noRecordingParams";

/// Used via `QnVirtualCameraResource`, internally, and in `axis_resource.cpp`
inline const QString kNoVideoSupport = "noVideoSupport";

/// Only used via `QnVirtualCameraResource`
inline const QString kPrimaryStreamConfiguration = "primaryStreamConfiguration";

/// Only used internally via `Camera` and internally
inline const QString kPtzTargetId = "ptzTargetId";

/// Used via `QnVirtualCameraResource`
inline const QString kRemoteArchiveMotionDetectionKey = "remoteArchiveMotionDetection";

/// Used via `QnVirtualCameraResource`
/**
 * Whether to download remote (i.e. on-camera) archives.
 */
inline const QString kRemoteArchiveSynchronizationEnabled = "remoteArchiveSynchronizationEnabled";

/// Only used via `QnVirtualCameraResource`
inline const QString kSecondaryStreamConfiguration = "secondaryStreamConfiguration";

/// Used internally and in Cloud
inline const QString kStorageInfo = "storageInfo";

/// Only used via `QnVirtualCameraResource`
inline const QString kStreamFpsSharing = "streamFpsSharing";

/// Used via `QnVirtualCameraResource`, internally, but also has tests in Desktop
/**
 * A string parameter with the following values allowed:
 * - softwaregrid. Software motion is calculated on the Server.
 * - hardwaregrid. Motion provided by the Camera.
 * softwaregrid and hardwaregrid can be combined into a comma-separated list.
 * The empty string means no motion is allowed.
 */
inline const QString kSupportedMotion = "supportedMotion";

/// Only used internally in `remote_archive_synchronizer.cpp`
/**
 * Whether time sync has been enabled once due to activation of remote archive sync.
 */
inline const QString kTimeSyncEnabledOnceDueToRemoteArchiveSync =
    "timeSyncEnabledOnceDueToRemoteArchiveSync";

/// Used via `QnVirtualCameraResource`, internally
inline const QString kTrustCameraTime = "trustCameraTime";

/// Used via `QnVirtualCameraResource`, internally
inline const QString kTwoWayAudioEnabled = "twoWayAudioEnabled";

/// Used via `QnVirtualCameraResource` and in `onvif_resource.cpp`
/**
 * To use or not to use onvif Media2 during camera initialization to fetch profiles.
 * The valid values are described in nx::core::resource::UsingOnvifMedia2Type.
 */
inline const QString kUseMedia2ToFetchProfiles = "useMedia2ToFetchProfiles";

/// Used internally, in `av_panoramic.cpp`, `onvif_resource.cpp` and other...
inline const QString kVideoLayout = "VideoLayout";

} // namespace device_properties

namespace resource_properties {

NX_VMS_API extern const QString kCustomGroupIdPropertyKey;

} // namespace resource_properties

} // namespace nx::vms::api

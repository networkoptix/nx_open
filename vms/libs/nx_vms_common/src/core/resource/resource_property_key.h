// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QString>

#include <nx/vms/api/data/resource_property_key.h>

/**
 * Contains the keys to treat properties of Resources (QnResource and its inheritors). Properties
 * are mutable attributes of a Resource. The type of any property is QString. Properties are stored
 * independently of the Resource, in a separate dictionary.
 *
 * Each Resource has a Resource type which can have default values for some properties.
 */
namespace ResourcePropertyKey {

namespace Server {

using namespace nx::vms::api::server_properties;

} // namespace Server

namespace User {

using nx::vms::api::user_properties::kUserSettings;

} // namespace User

namespace WebPage {

NX_VMS_COMMON_API extern const QString kSubtypeKey;

} // namespace WebPage

NX_VMS_COMMON_API extern const std::set<QString> kWriteOnlyNames;

// Storage
NX_VMS_COMMON_API extern const QString kPersistentStorageStatusFlagsKey;
NX_VMS_COMMON_API extern const QString kStorageArchiveMode;
NX_VMS_COMMON_API extern const QString kCloudStorageCapabilities;

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
NX_VMS_COMMON_API extern const QString kUnauthorizedTimeoutLimitsSec;
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
NX_VMS_COMMON_API extern const QString kFixWrongUriScheme;
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

// Disable the use of Onvif Profile-G services for remote archive management.
NX_VMS_COMMON_API extern const QString kOnvifRemoteArchiveDisabled;

// Onvif configuration parameters, such as:
//     - Additional flags that can be passed to gSOAP library.
//     - Security header policy for different requests
NX_VMS_COMMON_API extern const QString kOnvifConfigurationParameters;

NX_VMS_COMMON_API extern const QString kTimezoneConfiguration;

// Force disable ONVIF PullPoint Notification timestamp validation.
NX_VMS_COMMON_API extern const QString kOnvifIgnoreOutdatedNotifications;

// For some devices, if all Onvif recording events in the GetEventSearchResultsResponse message
// have the same time, this may indicate that the device has not processed the
// GetEventSearchResults request correctly and has only sent a subset of its events in the
// response.
NX_VMS_COMMON_API extern const QString kReRequestOnvifRecordingEventsIfAllEventsHaveSameTime;

// Not all devices support the "StartPoint", "EndPoint", and "IncludeStartState" parameters in the
// ONVIF FindEvents request. In this case, we must request all events since the epoch that do not
// use these parameters.
NX_VMS_COMMON_API extern const QString kOnvifFindEventsRequestSupportsTimeRangeParameters;

// Prefer native protocol for remote archive synchronization instead of Onvif Profile-G if available.
NX_VMS_COMMON_API extern const QString kPreferNativeApiForRemoteArchiveSynchronization;

// Force enable/disable check for compatible configurations in Onvif PTZ.
NX_VMS_COMMON_API extern const QString kOnvifForcePtzGetCompatibleConfigurationsSupported;

NX_VMS_COMMON_API extern const QString kUseFirstDefaultCredentials;

} // namespace ResourceDataKey

//-------------------------------------------------------------------------------------------------
// TODO: These constants should be moved somewhere else.
namespace Qn {

NX_VMS_COMMON_API extern const QString USER_FULL_NAME;
NX_VMS_COMMON_API extern const QString kResourceDataParamName;

} // namespace Qn

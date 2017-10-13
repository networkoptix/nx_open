#pragma once

#include <nx/utils/literal.h>

#include <QString>

namespace Qn
{
    //dynamic parameters of resource

    static const QString POSSIBLE_DEFAULT_CREDENTIALS_PARAM_NAME = lit("possibleDefaultCredentials");
    static const QString FORCED_DEFAULT_CREDENTIALS_PARAM_NAME = lit("forcedDefaultCredentials");
    static const QString PREFERRED_AUTH_SCHEME_PARAM_NAME = lit("preferredAuthScheme");
    static const QString HAS_DUAL_STREAMING_PARAM_NAME = lit("hasDualStreaming");
    static const QString DTS_PARAM_NAME = lit("dts");
    static const QString ANALOG_PARAM_NAME = lit("analog");
    static const QString IS_AUDIO_SUPPORTED_PARAM_NAME = lit("isAudioSupported");
    static const QString STREAM_FPS_SHARING_PARAM_NAME = lit("streamFpsSharing");
    static const QString MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");
    static const QString FORCED_AUDIO_SUPPORTED_PARAM_NAME = lit("forcedIsAudioSupported");
    static const QString MOTION_WINDOW_CNT_PARAM_NAME = lit("motionWindowCnt");
    static const QString MOTION_MASK_WINDOW_CNT_PARAM_NAME = lit("motionMaskWindowCnt");
    static const QString MOTION_SENS_WINDOW_CNT_PARAM_NAME = lit("motionSensWindowCnt");
    static const QString FORCED_IS_AUDIO_SUPPORTED_PARAM_NAME = lit("forcedIsAudioSupported");
    static const QString FORCE_BITRATE_PER_GOP = lit("bitratePerGOP");
    static const QString DESIRED_TRANSPORT_PARAM_NAME = lit("desiredTransport");
    static const QString ONVIF_INPUT_PORT_ALIASES_PARAM_NAME = lit("onvifInputPortAliases");
    static const QString ONVIF_MANUFACTURER_REPLACEMENT = lit("onvifManufacturerReplacement");
    /*!
        String parameter with following values allowed:\n
        - \a softwaregrid. Software motion calculated on mediaserver
        - \a hardwaregrid. Motion provided by camera
        \a softwaregrid and \a hardwaregrid can be combined as list split with comma
        Empty string means no motion is allowed
    */
    static const QString SUPPORTED_MOTION_PARAM_NAME = lit("supportedMotion");
    static const QString CAMERA_CREDENTIALS_PARAM_NAME = lit("credentials");
    static const QString CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME = lit("defaultCredentials");
    static const QString CAMERA_CAPABILITIES_PARAM_NAME = lit("cameraCapabilities");
    static const QString CAMERA_MEDIA_STREAM_LIST_PARAM_NAME = lit("mediaStreams");
    static const QString CAMERA_BITRATE_INFO_LIST_PARAM_NAME = lit("bitrateInfos");
    static const QString CAMERA_STREAM_URLS = lit("streamUrls");
    static const QString CAMERA_ALLOWED_FPS_LIST_PARAM_NAME = lit("allowedFpsList");
    static const QString TRUST_TO_VIDEO_SOURCE_SIZE_PARAM_NAME = lit("trustToVideoSourceSize");
    static const QString FPS_BASE_PARAM_NAME = lit("fpsBase"); //used if we need to control fps via encoding interval (fps when encoding interval is 1)
    static const QString CONTROL_FPS_VIA_ENCODING_INTERVAL_PARAM_NAME = lit("controlFpsViaEncodingInterval");
    static const QString FPS_BOUNDS_PARAM_NAME = lit("fpsBounds");
    static const QString USE_EXISTING_ONVIF_PROFILES_PARAM_NAME = lit("useExistingOnvifProfiles");
    static const QString CAMERA_AUDIO_CODEC_PARAM_NAME = lit("audioCodec");
    static const QString FORCED_PRIMARY_STREAM_RESOLUTION_PARAM_NAME = lit("forcedPrimaryStreamResolution");
    static const QString FORCED_SECONDARY_STREAM_RESOLUTION_PARAM_NAME = lit("forcedSecondaryStreamResolution");
    static const QString DO_NOT_CONFIGURE_CAMERA_PARAM_NAME = lit("doNotConfigureCamera");
    static const QString VIDEO_LAYOUT_PARAM_NAME = lit("VideoLayout");
    static const QString VIDEO_LAYOUT_PARAM_NAME2 = lit("videoLayout"); //used in resource_data.json
    static const QString DESIRED_H264_PROFILE_PARAM_NAME = lit("desiredH264Profile");
    static const QString FORCE_SINGLE_STREAM_PARAM_NAME = lit("forceSingleStream");
    static const QString HIGH_STREAM_AVAILABLE_BITRATES_PARAM_NAME = lit("highStreamAvailableBitrates");
    static const QString LOW_STREAM_AVAILABLE_BITRATES_PARAM_NAME = lit("lowStreamAvailableBitrates");
    static const QString HIGH_STREAM_BITRATE_BOUNDS_PARAM_NAME = lit("highStreamBitrateBounds");
    static const QString LOW_STREAM_BITRATE_BOUNDS_PARAM_NAME = lit("lowStreamBitrateBounds");
    static const QString kUnauthrizedTimeoutParamName = lit("unauthorizedTimeoutSec");
    static const QString TWO_WAY_AUDIO_PARAM_NAME = lit("2WayAudio");
    static const QString kCombinedSensorsDescriptionParamName = lit("combinedSensorsDescription");
    static const QString kAnalyticsDriversParamName = lit("analyticsDrivers");
    static const QString kGroupPlayParamName = lit("groupplay");

    static const QString kPrimaryStreamResolutionParamName = lit("primaryStreamResolution");
    static const QString kSecondaryStreamResolutionParamName = lit("secondaryStreamResolution");
    static const QString kPrimaryStreamCodecParamName = lit("primaryStreamCodec");
    static const QString kPrimaryStreamCodecProfileParamName = lit("primaryStreamCodecProfile");
    static const QString kSecondaryStreamCodecParamName = lit("secondaryStreamCodec");
    static const QString kSecondaryStreamCodecProfileParamName = lit("secondaryStreamCodecProfile");
    static const QString kPrimaryStreamGovLengthParamName = lit("primaryStreamGovLength");
    static const QString kSecondaryStreamGovLengthParamName = lit("secondaryStreamGovLength");
    static const QString kPrimaryStreamBitrateControlParamName = lit("primaryStreamBitrateControl");
    static const QString kSecondaryStreamBitrateControlParamName = lit("secondaryStreamBitrateControl");
    static const QString kPrimaryStreamBitrateParamName = lit("primaryStreamBitrate");
    static const QString kSecondaryStreamBitrateParamName = lit("secondaryStreamBitrate");
    static const QString kPrimaryStreamEntropyCodingParamName = lit("primaryStreamEntropyCoding");
    static const QString kSecondaryStreamEntropyCodingParamName = lit("secondaryStreamEntropyCoding");
    static const QString kSecondaryStreamFpsParamName = lit("secondaryStreamFps");

    static const QString ADVANCED_PARAMETER_OVERLOADS_PARAM_NAME = lit("advancedParameterOverloads");

    static const QString PRE_SRTEAM_CONFIGURE_REQUESTS_PARAM_NAME = lit("preStreamConfigureRequests");

    static const QString SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME = lit("shouldAppearAsSingleChannel");
    static const QString IGNORE_CAMERA_TIME_IF_BIG_JITTER_PARAM_NAME = lit("ignoreCameraTimeIfBigJitter");
    //!Contains QnCameraAdvancedParams in ubjson-serialized state
    static const QString CAMERA_ADVANCED_PARAMETERS = lit("cameraAdvancedParams");
    static const QString PROFILE_LEVEL_ID_PARAM_NAME = lit("profile-level-id");
    static const QString SPROP_PARAMETER_SETS_PARAM_NAME = lit("sprop-parameter-sets");
    static const QString FIRMWARE_PARAM_NAME = lit("firmware");
    static const QString IS_AUDIO_FORCED_PARAM_NAME = lit("forcedAudioStream");
	static const QString VIDEO_DISABLED_PARAM_NAME = lit("noVideoSupport");
    static const QString IO_SETTINGS_PARAM_NAME = lit("ioSettings");
    static const QString IO_CONFIG_PARAM_NAME = lit("ioConfigCapability");
    static const QString IO_PORT_DISPLAY_NAMES_PARAM_NAME = lit("ioDisplayName");
    static const QString IO_OVERLAY_STYLE_PARAM_NAME = lit("ioOverlayStyle");
    static const QString FORCE_ONVIF_PARAM_NAME = lit("forceONVIF");
    static const QString IGNORE_ONVIF_PARAM_NAME = lit("ignoreONVIF");
    static const QString PTZ_CAPABILITIES_PARAM_NAME = lit("ptzCapabilities");
    static const QString DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME = lit("disableNativePtzPresets");
	static const QString DW_REBRANDED_TO_ISD_MODEL = lit("isdDwCam");
    static const QString ONVIF_VENDOR_SUBTYPE = lit("onvifVendorSubtype");
    static const QString DO_NOT_ADD_VENDOR_TO_DEVICE_NAME = lit("doNotAddVendorToDeviceName");
    static const QString VIDEO_MULTIRESOURCE_CHANNEL_MAPPING_PARAM_NAME = lit("multiresourceVideoChannelMapping");
    static const QString NO_RECORDING_PARAMS_PARAM_NAME = lit("noRecordingParams");
    static const QString PARSE_ONVIF_NOTIFICATIONS_WITH_HTTP_READER = lit("parseOnvifNotificationsWithHttpReader");
    static const QString DISABLE_HEVC_PARAMETER_NAME = lit("disableHevc");

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
}

#ifndef QN_PARAM_H
#define QN_PARAM_H

#include <QString>

namespace Qn
{
    //dynamic parameters of resource

    static const QString POSSIBLE_DEFAULT_CREDENTIALS_PARAM_NAME = lit("possibleDefaultCredentials");
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
    static const QString CAMERA_ALLOWED_FPS_LIST_PARAM_NAME = lit("allowedFpsList");
    static const QString TRUST_TO_VIDEO_SOURCE_SIZE_PARAM_NAME = lit("trustToVideoSourceSize");
    static const QString FPS_BASE_PARAM_NAME = lit("fpsBase"); //used if we need to control fps via encoding interval (fps when encoding interval is 1)
    static const QString CONTROL_FPS_VIA_ENCODING_INTERVAL_PARAM_NAME = lit("controlFpsViaEncodingInterval");
    static const QString USE_EXISTING_ONVIF_PROFILES_PARAM_NAME = lit("useExistingOnvifProfiles");
    static const QString CAMERA_AUDIO_CODEC_PARAM_NAME = lit("audioCodec");
    static const QString FORCED_PRIMARY_STREAM_RESOLUTION_PARAM_NAME = lit("forcedPrimaryStreamResolution");
    static const QString FORCED_SECONDARY_STREAM_RESOLUTION_PARAM_NAME = lit("forcedSecondaryStreamResolution");
    static const QString DO_NOT_CONFIGURE_CAMERA_PARAM_NAME = lit("doNotConfigureCamera");
    static const QString VIDEO_LAYOUT_PARAM_NAME = lit("VideoLayout");
    static const QString VIDEO_LAYOUT_PARAM_NAME2 = lit("videoLayout"); //used in resource_data.json

    static const QString SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME = lit("shouldAppearAsSingleChannel");
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
    static const QString FORCE_ONVIF_PARAM_NAME = lit("forceONVIF");
    static const QString IGNORE_ONVIF_PARAM_NAME = lit("ignoreONVIF");
    static const QString DW_REBRANDED_TO_ISD_MODEL = lit("isdDwCam");
    static const QString ONVIF_VENDOR_SUBTYPE = lit("onvifVendorSubtype"); 
    static const QString DO_NOT_ADD_VENDOR_TO_DEVICE_NAME = lit("doNotAddVendorToDeviceName");
    static const QString VIDEO_MULTIRESOURCE_CHANNEL_MAPPING_PARAM_NAME = lit("multiresourceVideoChannelMapping");


    // Mediaserver info for Statistics
    static const QString CPU_ARCHITECTURE = lit("cpuArchitecture");
    static const QString CPU_MODEL_NAME = lit("cpuModelName");
    static const QString PHISICAL_MEMORY = lit("phisicalMemory");
    static const QString BETA = lit("beta");
    static const QString PUBLIC_IP = lit("publicIp");
    static const QString NETWORK_INTERFACES = lit("networkInterfaces");
    static const QString FULL_VERSION = lit("fullVersion");
    static const QString SYSTEM_RUNTIME = lit("systemRuntime");
    static const QString BOOKMARK_COUNT = lit("bookmarkCount");

    // Storage
    static const QString SPACE = lit("space");
}

#endif // QN_PARAM_H

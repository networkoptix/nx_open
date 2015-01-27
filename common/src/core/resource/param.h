#ifndef QN_PARAM_H
#define QN_PARAM_H

#include <QString>

namespace Qn
{
    //dynamic parameters of resource

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
    static const QString CAMERA_MEDIA_STREAM_PARAM_NAME_PREFIX = lit("mediaStream_");
    static const QString VIDEO_LAYOUT_PARAM_NAME = lit("VideoLayout");
    //!Contains XML describing camera parameteres. If empty, :camera_settings/camera_settings.xml file is used
    static const QString PHYSICAL_CAMERA_SETTINGS_XML_PARAM_NAME = lit("physicalCameraSettingsXml");
    //!ID of camera in camera_settings.xml or in parameter \a PHYSICAL_CAMERA_SETTINGS_XML_PARAM_NAME value
    static const QString CAMERA_SETTINGS_ID_PARAM_NAME = lit("cameraSettingsId");
}

#endif // QN_PARAM_H

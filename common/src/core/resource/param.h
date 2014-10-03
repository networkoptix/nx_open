#ifndef QN_PARAM_H
#define QN_PARAM_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

#include <utils/common/id.h>

#include "resource_fwd.h"


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
    //!possible values: softwaregrid,hardwaregrid
    static const QString SUPPORTED_MOTION_PARAM_NAME = lit("supportedMotion");
    static const QString CAMERA_CAPABILITIES_PARAM_NAME = lit("cameraCapabilities");
    static const QString CAMERA_MEDIA_STREAM_LIST_PARAM_NAME = lit("mediaStreams");

}

// TODO: #Elric #enum
enum QnDomain
{
    QnDomainMemory = 1,
    QnDomainDatabase = 2,
    QnDomainPhysical = 4
};

struct QN_EXPORT QnParamType
{
    QnParamType() {}

    QString name;
    QString default_value;
};

#endif // QN_PARAM_H

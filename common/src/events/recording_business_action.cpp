#include "recording_business_action.h"

namespace BusinessActionParameters {

    static QLatin1String fps("fps");
    static QLatin1String quality("quality");
    static QLatin1String duration("duration");
    static QLatin1String before("before");
    static QLatin1String after("after");

    int getFps(const QnBusinessParams &params) {
        return params.value(fps, 10).toInt();
    }

    void setFps(QnBusinessParams* params, int value) {
        (*params)[fps] = value;
    }

    QnStreamQuality getStreamQuality(const QnBusinessParams &params) {
        return (QnStreamQuality)params.value(quality, (int)QnQualityHighest).toInt();
    }

    void setStreamQuality(QnBusinessParams* params, QnStreamQuality value) {
        (*params)[quality] = (int)value;
    }

    int getRecordDuration(const QnBusinessParams &params) {
        return params.value(duration, 0).toInt();
    }

    void setRecordDuration(QnBusinessParams* params, int value) {
        (*params)[duration] = value;
    }

    int getRecordBefore(const QnBusinessParams &params) {
        return params.value(before, 0).toInt();
    }

    void setRecordBefore(QnBusinessParams* params, int value) {
        (*params)[before] = value;
    }

    int getRecordAfter(const QnBusinessParams &params) {
        return params.value(after, 0).toInt();
    }

    void setRecordAfter(QnBusinessParams* params, int value)  {
        (*params)[after] = value;
    }

}

QnRecordingBusinessAction::QnRecordingBusinessAction(const QnBusinessParams &runtimeParams):
    base_type(BusinessActionType::BA_CameraRecording, runtimeParams)
{
}

int QnRecordingBusinessAction::getFps() const {
    return BusinessActionParameters::getFps(getParams());
}

QnStreamQuality QnRecordingBusinessAction::getStreamQuality() const {
    return BusinessActionParameters::getStreamQuality(getParams());
}

int QnRecordingBusinessAction::getRecordDuration() const {
    return BusinessActionParameters::getRecordDuration(getParams());
}

int QnRecordingBusinessAction::getRecordBefore() const {
    return BusinessActionParameters::getRecordBefore(getParams());
}

int QnRecordingBusinessAction::getRecordAfter() const {
    return BusinessActionParameters::getRecordAfter(getParams());
}

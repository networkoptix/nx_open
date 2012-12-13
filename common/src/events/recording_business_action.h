#ifndef __RECORDING_BUSINESS_ACTION_H__
#define __RECORDING_BUSINESS_ACTION_H__

#include "core/resource/media_resource.h"
#include "abstract_business_action.h"

class QnRecordingBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    QnRecordingBusinessAction();

    void setFps(int value) { m_params[QLatin1String("fps")] = value; }
    int getFps() const { return m_params.value(QLatin1String("fps")).toInt(); }

    void setStreamQuality(QnStreamQuality value) { m_params[QLatin1String("quality")] = (int) value; }
    QnStreamQuality getStreamQuality() const { return (QnStreamQuality) m_params.value(QLatin1String("quality")).toInt(); }

    void setRecordDuration(int value) { m_params[QLatin1String("duration")] = value;}
    int getRecordDuration() const { return m_params.value(QLatin1String("duration")).toInt(); }
private:
};

typedef QSharedPointer<QnRecordingBusinessAction> QnRecordingBusinessActionPtr;

#endif // __RECORDING_BUSINESS_ACTION_H__

#ifndef __RECORDING_BUSINESS_ACTION_H__
#define __RECORDING_BUSINESS_ACTION_H__

#include "toggle_business_action.h"
#include "core/resource/media_resource.h"

class QnRecordingBusinessAction: public QnToggleBusinessAction
{
public:
    QnRecordingBusinessAction();

    virtual QByteArray QnRecordingBusinessAction::serialize() override;
    virtual bool QnRecordingBusinessAction::deserialize(const QByteArray& data) override;

    void setFps(int value) { m_params["fps"] = value; }
    int getFps() const { return m_params.value("fps").toInt(); }

    void setStreamQuality(QnStreamQuality value) { m_params["quality"] = (int) value; }
    QnStreamQuality getStreamQuality() const { return (QnStreamQuality) m_params.value("quality").toInt(); }

    void setRecordDuration(int value) { m_params["duration"] = value;}
    int getRecordDuration() const { return m_params.value("duration").toInt(); }
private:
};

typedef QSharedPointer<QnRecordingBusinessAction> QnRecordingBusinessActionPtr;

#endif // __RECORDING_BUSINESS_ACTION_H__

#ifndef __RECORDING_BUSINESS_ACTION_H__
#define __RECORDING_BUSINESS_ACTION_H__

#include "core/resource/media_resource.h"
#include "abstract_business_action.h"

namespace BusinessActionParameters {

    int getFps(const QnBusinessParams &params);
    void setFps(QnBusinessParams* params, int value);

    QnStreamQuality getStreamQuality(const QnBusinessParams &params);
    void setStreamQuality(QnBusinessParams* params, QnStreamQuality value);

    int getRecordDuration(const QnBusinessParams &params);
    void setRecordDuration(QnBusinessParams* params, int value);

}

class QnRecordingBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnRecordingBusinessAction(const QnBusinessParams &runtimeParams);

    int getFps() const;
    QnStreamQuality getStreamQuality() const;
    int getRecordDuration() const;
};

typedef QSharedPointer<QnRecordingBusinessAction> QnRecordingBusinessActionPtr;

#endif // __RECORDING_BUSINESS_ACTION_H__

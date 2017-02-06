#ifndef __RECORDING_BUSINESS_ACTION_H__
#define __RECORDING_BUSINESS_ACTION_H__

#include <business/actions/abstract_business_action.h>

#include <core/resource/resource_fwd.h>

class QnRecordingBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams);

    int getFps() const;
    Qn::StreamQuality getStreamQuality() const;
    int getRecordDuration() const;
    int getRecordAfter() const;
};

typedef QSharedPointer<QnRecordingBusinessAction> QnRecordingBusinessActionPtr; // TODO: #Elric move to fwd header.

#endif // __RECORDING_BUSINESS_ACTION_H__

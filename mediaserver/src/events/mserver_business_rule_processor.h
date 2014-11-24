#ifndef __MSERVER_BUSINESS_RULE_PROCESSOR_H_
#define __MSERVER_BUSINESS_RULE_PROCESSOR_H_

#include <business/business_rule_processor.h>
#include <business/actions/recording_business_action.h>
#include "events_db.h"

#include <core/resource/resource_fwd.h>

/*
* QnMServerBusinessRuleProcessor can execute business actions
*/

class QnMServerBusinessRuleProcessor: public QnBusinessRuleProcessor
{
    Q_OBJECT
public:
    QnMServerBusinessRuleProcessor();
    virtual ~QnMServerBusinessRuleProcessor();

    virtual QnUuid getGuid() const override;
    
    /*
    * How long to keep event log in usecs
    */
    void setEventLogPeriod(qint64 periodUsec);

protected:
    QImage getEventScreenshot(const QnBusinessEventParameters& params, QSize dstSize) const override;

protected slots:
    virtual bool executeActionInternal(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res) override;
    void onRemoveResource(const QnResourcePtr &resource);

private:
    bool executeRecordingAction(const QnRecordingBusinessActionPtr& action, const QnResourcePtr& res);
    bool executePanicAction(const QnPanicBusinessActionPtr& action);
    bool triggerCameraOutput(const QnCameraOutputBusinessActionPtr& action, const QnResourcePtr& resource);
};

#endif // __MSERVER_BUSINESS_RULE_PROCESSOR_H_

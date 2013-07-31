#ifndef __MSERVER_BUSINESS_RULE_PROCESSOR_H_
#define __MSERVER_BUSINESS_RULE_PROCESSOR_H_

#include <business/business_rule_processor.h>
#include <business/actions/recording_business_action.h>
#include "events_db.h"

/*
* QnMServerBusinessRuleProcessor can execute business actions
*/

class QnMServerBusinessRuleProcessor: public QnBusinessRuleProcessor
{
    Q_OBJECT
public:
    QnMServerBusinessRuleProcessor();
    virtual ~QnMServerBusinessRuleProcessor();

    virtual QString getGuid() const override;
    
    /*
    * How long to keep event log in usecs
    */
    void setEventLogPeriod(qint64 periodUsec);

protected:
    QImage getEventScreenshot(const QnBusinessEventParameters& params, QSize dstSize) const override;

protected slots:
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res) override;
    void onRemoveResource(const QnResourcePtr &resource);

private:
    bool executeRecordingAction(QnRecordingBusinessActionPtr action, QnResourcePtr res);
    bool executePanicAction(QnPanicBusinessActionPtr action);
    bool triggerCameraOutput(const QnCameraOutputBusinessActionPtr& action, QnResourcePtr resource);
};

#endif // __MSERVER_BUSINESS_RULE_PROCESSOR_H_

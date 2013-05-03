#ifndef __MSERVER_BUSINESS_RULE_PROCESSOR_H_
#define __MSERVER_BUSINESS_RULE_PROCESSOR_H_

#include <business/business_rule_processor.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/panic_business_action.h>
#include "events_db.h"

/*
* QnMServerBusinessRuleProcessor can execute business actions
*/

class QnMServerBusinessRuleProcessor: public QnBusinessRuleProcessor
{
public:
    QnMServerBusinessRuleProcessor();
    virtual ~QnMServerBusinessRuleProcessor();

    virtual QString getGuid() const override;
    
    /*
    * How long to keep event log in usecs
    */
    void setEventLogPeriod(qint64 periodUsec);

    const QnEventsDB& getDB();

protected slots:
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res) override;
private:
    bool executeRecordingAction(QnRecordingBusinessActionPtr action, QnResourcePtr res);
    bool executePanicAction(QnPanicBusinessActionPtr action);
    bool triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action, QnResourcePtr resource );
private:
    QnEventsDB m_db;
};

#endif // __MSERVER_BUSINESS_RULE_PROCESSOR_H_

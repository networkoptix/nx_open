#ifndef __MSERVER_BUSINESS_RULE_PROCESSOR_H_
#define __MSERVER_BUSINESS_RULE_PROCESSOR_H_

#include "events/business_rule_processor.h"
#include "events/recording_business_action.h"

/*
* QnMServerBusinessRuleProcessor can execute business actions
*/

class QnMServerBusinessRuleProcessor: public QnBusinessRuleProcessor
{
public:
    virtual QString getGuid() const override;
protected:
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action) override;
private:
    bool executeRecordingAction(QnRecordingBusinessActionPtr action);
};

#endif // __MSERVER_BUSINESS_RULE_PROCESSOR_H_

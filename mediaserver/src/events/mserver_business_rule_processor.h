#ifndef __MSERVER_BUSINESS_RULE_PROCESSOR_H_
#define __MSERVER_BUSINESS_RULE_PROCESSOR_H_

#include <QtSql>

#include <business/business_rule_processor.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/panic_business_action.h>

/*
* QnMServerBusinessRuleProcessor can execute business actions
*/

class QnMServerBusinessRuleProcessor: public QnBusinessRuleProcessor
{
public:
    QnMServerBusinessRuleProcessor();
    virtual ~QnMServerBusinessRuleProcessor();

    virtual QString getGuid() const override;
protected slots:
    virtual bool executeActionInternal(QnAbstractBusinessActionPtr action, QnResourcePtr res) override;
private:
    bool executeRecordingAction(QnRecordingBusinessActionPtr action, QnResourcePtr res);
    bool executePanicAction(QnPanicBusinessActionPtr action);
    bool triggerCameraOutput( const QnCameraOutputBusinessActionPtr& action, QnResourcePtr resource );
    void saveActionToDB(QnAbstractBusinessActionPtr action);
private:
    QSqlDatabase m_sdb;
};

#endif // __MSERVER_BUSINESS_RULE_PROCESSOR_H_

#ifndef __MSERVER_BUSINESS_RULE_PROCESSOR_H_
#define __MSERVER_BUSINESS_RULE_PROCESSOR_H_

#include <business/business_rule_processor.h>
#include <business/actions/recording_business_action.h>

#include <core/resource/resource_fwd.h>

/*
* QnMServerBusinessRuleProcessor can execute business actions
*/

class QnMServerBusinessRuleProcessor: public QnBusinessRuleProcessor
{
    Q_OBJECT
public:
    QnMServerBusinessRuleProcessor(QnCommonModule* commonModule);
    virtual ~QnMServerBusinessRuleProcessor();

    virtual QnUuid getGuid() const override;

    /*
    * How long to keep event log in usecs
    */
    void setEventLogPeriod(qint64 periodUsec);
    virtual void prepareAdditionActionParams(const QnAbstractBusinessActionPtr& action) override;
protected slots:
    virtual bool executeActionInternal(const QnAbstractBusinessActionPtr& action) override;
    void onRemoveResource(const QnResourcePtr &resource);

private:
    bool executeRecordingAction(const QnRecordingBusinessActionPtr& action);
    bool executePanicAction(const QnPanicBusinessActionPtr& action);
    bool triggerCameraOutput(const QnCameraOutputBusinessActionPtr& action);
    bool executeBookmarkAction(
        const QnAbstractBusinessActionPtr &action,
        bool createBookmark);
    bool executeHttpRequestAction(const QnAbstractBusinessActionPtr& action);
    bool executePtzAction(const QnAbstractBusinessActionPtr& action);
    bool executeSayTextAction(const QnAbstractBusinessActionPtr& action);
    bool executePlaySoundAction(const QnAbstractBusinessActionPtr& action);

private:
    class SendEmailAggregationKey
    {
    public:
        QnBusiness::EventType eventType;
        QString recipients;

        SendEmailAggregationKey()
            :
            eventType( QnBusiness::UndefinedEvent )
        {
        }

        SendEmailAggregationKey(
            QnBusiness::EventType _eventType,
            QString _recipients )
            :
        eventType( _eventType ),
            recipients( _recipients )
        {
        }

        bool operator<( const SendEmailAggregationKey& right ) const
        {
            if( eventType < right.eventType )
                return true;
            if( right.eventType < eventType )
                return false;
            return recipients < right.recipients;
        }
    };

    class SendEmailAggregationData
    {
    public:
        QnSendMailBusinessActionPtr action;
        quint64 periodicTaskID;
        int eventCount;

        SendEmailAggregationData() : periodicTaskID(0), eventCount(0) {}
    };


    QMap<QnUuid, qint64> m_runningBookmarkActions;
    QScopedPointer<EmailManagerImpl> m_emailManager;
    QMap<SendEmailAggregationKey, SendEmailAggregationData> m_aggregatedEmails;
    QThreadPool m_emailThreadPool;
private:
    bool sendMail(const QnSendMailBusinessActionPtr& action );
    void sendAggregationEmail( const SendEmailAggregationKey& aggregationKey );
    bool sendMailInternal(const QnSendMailBusinessActionPtr& action, int aggregatedResCount );
    void sendEmailAsync(QnSendMailBusinessActionPtr action, QStringList recipients, int aggregatedResCount);

    /**
     * This method is called once per action, calculates all recipients and packs them into
     * emailAddress action parameter with new delimiter.
     */
    void updateRecipientsList(const QnSendMailBusinessActionPtr& action) const;

    QByteArray getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize) const;

    QVariantMap eventDescriptionMap(
        const QnAbstractBusinessActionPtr& action,
        const QnBusinessAggregationInfo &aggregationInfo,
        QnEmailAttachmentList& attachments) const;

    QVariantMap eventDetailsMap(
        const QnAbstractBusinessActionPtr& action,
        const QnInfoDetail& aggregationData,
        Qn::ResourceInfoLevel detailLevel,
        bool addSubAggregationData = true ) const;

    QVariantList aggregatedEventDetailsMap(const QnAbstractBusinessActionPtr& action,
        const QnBusinessAggregationInfo& aggregationInfo,
        Qn::ResourceInfoLevel detailLevel) const;

    QVariantList aggregatedEventDetailsMap(
        const QnAbstractBusinessActionPtr& action,
        const QList<QnInfoDetail>& aggregationDetailList,
        Qn::ResourceInfoLevel detailLevel) const;
};

#endif // __MSERVER_BUSINESS_RULE_PROCESSOR_H_

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
    QnMServerBusinessRuleProcessor();
    virtual ~QnMServerBusinessRuleProcessor();

    virtual QnUuid getGuid() const override;
    
    /*
    * How long to keep event log in usecs
    */
    void setEventLogPeriod(qint64 periodUsec);

protected slots:
    virtual bool executeActionInternal(const QnAbstractBusinessActionPtr& action, const QnResourcePtr& res) override;
    void onRemoveResource(const QnResourcePtr &resource);

private:
    bool executeRecordingAction(const QnRecordingBusinessActionPtr& action, const QnResourcePtr& res);
    bool executePanicAction(const QnPanicBusinessActionPtr& action);
    bool triggerCameraOutput(const QnCameraOutputBusinessActionPtr& action, const QnResourcePtr& resource);
    bool executeBookmarkAction(const QnAbstractBusinessActionPtr &action, const QnResourcePtr &resource);

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
private:
    bool sendMail(const QnSendMailBusinessActionPtr& action );
    void sendAggregationEmail( const SendEmailAggregationKey& aggregationKey );
    bool sendMailInternal(const QnSendMailBusinessActionPtr& action, int aggregatedResCount );
    void sendEmailAsync(const ec2::ApiEmailData& data);
    QString formatEmailList(const QStringList& value) const;

    static QByteArray getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize);

    static QVariantHash eventDescriptionMap(
        const QnAbstractBusinessActionPtr& action, 
        const QnBusinessAggregationInfo &aggregationInfo, 
        QnEmailAttachmentList& attachments, 
        bool useIp);

    static QVariantHash eventDetailsMap(
        const QnAbstractBusinessActionPtr& action,
        const QnInfoDetail& aggregationData,
        bool useIp,
        bool addSubAggregationData = true );

    static QVariantList aggregatedEventDetailsMap(const QnAbstractBusinessActionPtr& action,
        const QnBusinessAggregationInfo& aggregationInfo,
        bool useIp);
    static QVariantList aggregatedEventDetailsMap(
        const QnAbstractBusinessActionPtr& action,
        const QList<QnInfoDetail>& aggregationDetailList,
        bool useIp );
};

#endif // __MSERVER_BUSINESS_RULE_PROCESSOR_H_

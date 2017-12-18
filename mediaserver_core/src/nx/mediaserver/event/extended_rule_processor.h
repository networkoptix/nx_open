#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/mediaserver/event/rule_processor.h>
#include <nx/vms/event/actions/actions_fwd.h>

namespace nx {
namespace mediaserver {
namespace event {

/*
* ExtendedRuleProcessor can execute business actions
*/

class ExtendedRuleProcessor: public RuleProcessor
{
    Q_OBJECT
    using base_type = RuleProcessor;

public:
    ExtendedRuleProcessor(QnCommonModule* commonModule);
    virtual ~ExtendedRuleProcessor();

    virtual QnUuid getGuid() const override;

    virtual void prepareAdditionActionParams(const vms::event::AbstractActionPtr& action) override;

protected slots:
    virtual bool executeActionInternal(const vms::event::AbstractActionPtr& action) override;
    void onRemoveResource(const QnResourcePtr &resource);

private:
    bool executeRecordingAction(const vms::event::RecordingActionPtr& action);
    bool executePanicAction(const vms::event::PanicActionPtr& action);
    bool triggerCameraOutput(const vms::event::CameraOutputActionPtr& action);
    bool executeBookmarkAction(const vms::event::AbstractActionPtr &action);
    bool executeHttpRequestAction(const vms::event::AbstractActionPtr& action);
    bool executePtzAction(const vms::event::AbstractActionPtr& action);
    bool executeSayTextAction(const vms::event::AbstractActionPtr& action);
    bool executePlaySoundAction(const vms::event::AbstractActionPtr& action);

private:
    class SendEmailAggregationKey
    {
    public:
        vms::event::EventType eventType;
        QString recipients;

        SendEmailAggregationKey():
            eventType(vms::event::undefinedEvent)
        {
        }

        SendEmailAggregationKey(vms::event::EventType _eventType, QString _recipients):
            eventType(_eventType),
            recipients(_recipients)
        {
        }

        bool operator < (const SendEmailAggregationKey& right) const
        {
            if (eventType < right.eventType)
                return true;
            if (right.eventType < eventType)
                return false;
            return recipients < right.recipients;
        }
    };

    class SendEmailAggregationData
    {
    public:
        vms::event::SendMailActionPtr action;
        quint64 periodicTaskID;
        int eventCount;

        SendEmailAggregationData() : periodicTaskID(0), eventCount(0) {}
    };

    QMap<QnUuid, qint64> m_runningBookmarkActions;
    QScopedPointer<EmailManagerImpl> m_emailManager;
    QMap<SendEmailAggregationKey, SendEmailAggregationData> m_aggregatedEmails;
    QThreadPool m_emailThreadPool;
private:
    bool sendMail(const vms::event::SendMailActionPtr& action);
    void sendAggregationEmail(const SendEmailAggregationKey& aggregationKey);
    bool sendMailInternal(const vms::event::SendMailActionPtr& action, int aggregatedResCount);
    void sendEmailAsync(vms::event::SendMailActionPtr action, QStringList recipients, int aggregatedResCount);

    /**
     * This method is called once per action, calculates all recipients and packs them into
     * emailAddress action parameter with new delimiter.
     */
    void updateRecipientsList(const vms::event::SendMailActionPtr& action) const;

    QByteArray getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec, QSize dstSize) const;

    QVariantMap eventDescriptionMap(
        const vms::event::AbstractActionPtr& action,
        const vms::event::AggregationInfo &aggregationInfo,
        QnEmailAttachmentList& attachments) const;

    QVariantMap eventDetailsMap(
        const vms::event::AbstractActionPtr& action,
        const vms::event::InfoDetail& aggregationData,
        Qn::ResourceInfoLevel detailLevel,
        bool addSubAggregationData = true) const;

    QVariantList aggregatedEventDetailsMap(const vms::event::AbstractActionPtr& action,
        const vms::event::AggregationInfo& aggregationInfo,
        Qn::ResourceInfoLevel detailLevel) const;

    QVariantList aggregatedEventDetailsMap(
        const vms::event::AbstractActionPtr& action,
        const QList<vms::event::InfoDetail>& aggregationDetailList,
        Qn::ResourceInfoLevel detailLevel) const;
};

} // namespace event
} // namespace mediaserver
} // namespace nx

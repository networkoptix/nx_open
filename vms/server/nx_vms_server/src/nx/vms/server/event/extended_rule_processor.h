#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/event/rule_processor.h>
#include <nx/vms/event/actions/actions_fwd.h>
#include <nx/email/email_manager_impl.h>
#include <nx/utils/elapsed_timer.h>

class QnMediaServerModule;

namespace nx {
namespace vms::server {
namespace event {

/*
* ExtendedRuleProcessor can execute business actions
*/

class ExtendedRuleProcessor: public RuleProcessor
{
    Q_OBJECT
    using base_type = RuleProcessor;

public:
    ExtendedRuleProcessor(QnMediaServerModule* serverModule);
    virtual ~ExtendedRuleProcessor();

    virtual QnUuid getGuid() const override;

    virtual void prepareAdditionActionParams(const vms::event::AbstractActionPtr& action) override;

    void stop();

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
    class SendEmailAggregationData
    {
    public:
        vms::event::SendMailActionPtr action;
        quint64 periodicTaskID = 0;
        nx::utils::ElapsedTimer lastMailTime;
    };

    QMap<QnUuid, qint64> m_runningBookmarkActions;
    QScopedPointer<EmailManagerImpl> m_emailManager;
    QMap<QnUuid, SendEmailAggregationData> m_aggregatedEmails;
    QThreadPool m_emailThreadPool;

private:
    bool sendMail(const vms::event::SendMailActionPtr& action);
    void sendAggregationEmail(const QnUuid& ruleId);
    bool sendMailInternal(const vms::event::SendMailActionPtr& action, int aggregatedResCount);
    void sendEmailAsync(vms::event::SendMailActionPtr action, QStringList recipients, int aggregatedResCount);

    /**
     * This method is called once per action, calculates all recipients and packs them into
     * emailAddress action parameter with new delimiter.
     */
    void updateRecipientsList(const vms::event::SendMailActionPtr& action) const;

    struct TimespampedFrame
    {
        qint64 timestampUsec = 0;
        QByteArray frame;
    };

    TimespampedFrame getEventScreenshotEncoded(const QnUuid& id, qint64 timestampUsec,
        QSize dstSize) const;

    QVariantMap eventDescriptionMap(
        const vms::event::AbstractActionPtr& action,
        const vms::event::AggregationInfo &aggregationInfo,
        EmailManagerImpl::AttachmentList& attachments) const;

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
} // namespace vms::server
} // namespace nx

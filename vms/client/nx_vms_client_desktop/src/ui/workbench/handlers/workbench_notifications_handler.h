#pragma once

#include <QtCore/QObject>

#include <api/model/kvpair.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <core/resource/resource_fwd.h>
#include <health/system_health.h>
#include <ui/workbench/workbench_state_manager.h>
#include <nx_ec/ec_api.h>

#include <utils/common/connective.h>

class QnBusinessEventsFilterResourcePropertyAdaptor;

class QnWorkbenchNotificationsHandler:
    public Connective<QObject>,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit QnWorkbenchNotificationsHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchNotificationsHandler();

    void addSystemHealthEvent(QnSystemHealth::MessageType message);
    void addSystemHealthEvent(QnSystemHealth::MessageType message,
        const nx::vms::event::AbstractActionPtr& action);

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

signals:
    void systemHealthEventAdded( QnSystemHealth::MessageType message, const QVariant& params);
    void systemHealthEventRemoved( QnSystemHealth::MessageType message, const QVariant& params);

    void notificationAdded(const nx::vms::event::AbstractActionPtr& action);
    void notificationRemoved(const nx::vms::event::AbstractActionPtr& action);

    void cleared();

public slots:
    void clear();

private slots:
    void at_context_userChanged();
    void at_eventManager_actionReceived(const nx::vms::event::AbstractActionPtr& action);

private:
    void addNotification(const nx::vms::event::AbstractActionPtr& businessAction);

    void setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible);
    void setSystemHealthEventVisible(QnSystemHealth::MessageType message,
        const QnResourcePtr& resource, bool visible);

    void setSystemHealthEventVisibleInternal(QnSystemHealth::MessageType message,
        const QVariant& params, bool visible);

    void handleAcknowledgeEventAction();

    void handleFullscreenCameraAction(const nx::vms::event::AbstractActionPtr& action);
    void handleExitFullscreenAction(const nx::vms::event::AbstractActionPtr& action);

private:
    QnBusinessEventsFilterResourcePropertyAdaptor* m_adaptor;
};

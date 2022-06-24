// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <health/system_health.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <ui/workbench/workbench_state_manager.h>

#include <utils/common/connective.h>

class QnBusinessEventsFilterResourcePropertyAdaptor;

namespace nx::vms::rules { class NotificationAction; }
namespace nx::vms::client::desktop { class CrossSystemNotificationsListener; }

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

    void setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible);
    void setSystemHealthEventVisible(QnSystemHealth::MessageType message,
        const QnResourcePtr& resource, bool visible);

signals:
    void systemHealthEventAdded(QnSystemHealth::MessageType message, const QVariant& params);
    void systemHealthEventRemoved(QnSystemHealth::MessageType message, const QVariant& params);

    void notificationAdded(const nx::vms::event::AbstractActionPtr& action);
    void notificationRemoved(const nx::vms::event::AbstractActionPtr& action);
    void notificationActionReceived(
        const QSharedPointer<nx::vms::rules::NotificationAction>& notificationAction);

    void cleared();

public slots:
    void clear();

private slots:
    void at_context_userChanged();
    void at_eventManager_actionReceived(const nx::vms::event::AbstractActionPtr& action);

private:
    void addNotification(const nx::vms::event::AbstractActionPtr& businessAction);

    void setSystemHealthEventVisibleInternal(QnSystemHealth::MessageType message,
        const QVariant& params, bool visible);

    void handleAcknowledgeEventAction();

    void handleFullscreenCameraAction(const nx::vms::event::AbstractActionPtr& action);
    void handleExitFullscreenAction(const nx::vms::event::AbstractActionPtr& action);

private:
    QnBusinessEventsFilterResourcePropertyAdaptor* m_adaptor;
    std::unique_ptr<nx::vms::client::desktop::CrossSystemNotificationsListener> m_listener;
};

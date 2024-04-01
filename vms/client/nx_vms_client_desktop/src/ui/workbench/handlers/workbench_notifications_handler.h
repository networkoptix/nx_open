// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/event_fwd.h>
#include <ui/workbench/workbench_state_manager.h>

class QnWorkbenchNotificationsHandler:
    public QObject,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QObject;

public:
    using MessageType = nx::vms::common::system_health::MessageType;

    explicit QnWorkbenchNotificationsHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchNotificationsHandler() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    void setSystemHealthEventVisible(
        MessageType message, const QnResourcePtr& resource, bool visible);

signals:
    void systemHealthEventAdded(MessageType message, const QVariant& params);
    void systemHealthEventRemoved(MessageType message, const QVariant& params);

    void notificationAdded(const nx::vms::event::AbstractActionPtr& action);
    void notificationRemoved(const nx::vms::event::AbstractActionPtr& action);
    void cleared();

private:
    void clear();

    void at_context_userChanged();
    void at_businessActionReceived(const nx::vms::event::AbstractActionPtr& action);

    void addNotification(const nx::vms::event::AbstractActionPtr& businessAction);
    void removeNotification(const nx::vms::event::AbstractActionPtr& action);

    void addSystemHealthEvent(
        MessageType message,
        const nx::vms::event::AbstractActionPtr& action);

    void setSystemHealthEventVisibleInternal(
        MessageType message,
        const QVariant& params,
        bool visible);

    void handleAcknowledgeEventAction();

    void handleFullscreenCameraAction(const nx::vms::event::AbstractActionPtr& action);
    void handleExitFullscreenAction(const nx::vms::event::AbstractActionPtr& action);

    void showSplash(const nx::vms::event::AbstractActionPtr &action);
};

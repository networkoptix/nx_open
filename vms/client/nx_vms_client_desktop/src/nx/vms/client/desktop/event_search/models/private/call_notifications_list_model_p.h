// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "../call_notifications_list_model.h"
#include "sound_controller.h"

namespace nx::vms::api { struct SiteHealthMessage; }

namespace nx::vms::client::desktop {

class CallNotificationsListModel::Private:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(CallNotificationsListModel* q);
    virtual ~Private() override;

private:
    void addNotification(const nx::vms::api::SiteHealthMessage& message);
    void removeNotification(const nx::vms::api::SiteHealthMessage& message);
    QString tooltip(
        const nx::vms::api::SiteHealthMessage& message,
        const QnVirtualCameraResourcePtr& camera) const;

private:
    CallNotificationsListModel* const q = nullptr;
    SoundController m_soundController;
};

} // namespace nx::vms::client::desktop

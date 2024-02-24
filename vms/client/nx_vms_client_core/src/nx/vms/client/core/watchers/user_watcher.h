// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API UserWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    /* This property should remain read-only for QML! */
    Q_PROPERTY(QString userName READ userName NOTIFY userNameChanged)

public:
    UserWatcher(SystemContext* systemContext, QObject* parent = nullptr);

    const QnUserResourcePtr& user() const;
    QString userName() const;

private:
    void setUser(const QnUserResourcePtr& currentUser);

signals:
    void userChanged(const QnUserResourcePtr& user);
    void userNameChanged();

private:
    QnUserResourcePtr m_user;
};

} // namespace nx::vms::client::core

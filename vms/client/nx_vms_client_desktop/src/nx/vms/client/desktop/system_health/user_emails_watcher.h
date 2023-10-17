// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class UserEmailsWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit UserEmailsWatcher(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~UserEmailsWatcher();

    QnUserResourceSet usersWithInvalidEmail() const;

signals:
    void userSetChanged();

private:
    void setUsersWithInvalidEmail(QnUserResourceSet value);

private:
    QnUserResourceSet m_usersWithInvalidEmail;
};

} // namespace nx::vms::client::desktop

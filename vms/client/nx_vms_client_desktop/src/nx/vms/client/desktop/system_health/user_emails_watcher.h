// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QHash>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class UserEmailsWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit UserEmailsWatcher(QObject* parent = nullptr);
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

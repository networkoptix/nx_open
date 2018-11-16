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
    static bool userHasInvalidEmail(const QnUserResourcePtr& user);
    void checkUser(const QnUserResourcePtr& user);
    void checkAll();

private:
    QnUserResourceSet m_usersWithInvalidEmail;
    bool m_processRuntimeUpdates = false;
};

} // namespace nx::vms::client::desktop

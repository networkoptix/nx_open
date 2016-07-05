#include "cloud_system_information_watcher.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <client_core/client_core_settings.h>
#include <watchers/cloud_status_watcher.h>

class QnCloudSystemInformationWatcherPrivate : public QObject
{
    QnCloudSystemInformationWatcher* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudSystemInformationWatcher)
public:
    QString systemId;
    QString ownerEmail;
    QString ownerFullName;

public:
    QnCloudSystemInformationWatcherPrivate(QnCloudSystemInformationWatcher* parent);
    void updateInformation(const QnCloudSystem& system);
};

QnCloudSystemInformationWatcher::QnCloudSystemInformationWatcher(QObject* parent) :
    base_type(parent),
    d_ptr(new QnCloudSystemInformationWatcherPrivate(this))
{
    connect(this, &QnCloudSystemInformationWatcher::ownerEmailChanged,
            this, &QnCloudSystemInformationWatcher::ownerDescriptionChanged);
    connect(this, &QnCloudSystemInformationWatcher::ownerFullNameChanged,
            this, &QnCloudSystemInformationWatcher::ownerDescriptionChanged);
}

QnCloudSystemInformationWatcher::~QnCloudSystemInformationWatcher()
{
}

QString QnCloudSystemInformationWatcher::systemId() const
{
    Q_D(const QnCloudSystemInformationWatcher);
    return d->systemId;
}

QString QnCloudSystemInformationWatcher::ownerEmail() const
{
    Q_D(const QnCloudSystemInformationWatcher);
    return d->ownerEmail;
}

QString QnCloudSystemInformationWatcher::ownerFullName() const
{
    Q_D(const QnCloudSystemInformationWatcher);
    return d->ownerFullName;
}

QString QnCloudSystemInformationWatcher::ownerDescription() const
{
    Q_D(const QnCloudSystemInformationWatcher);
    if (d->ownerEmail.isEmpty() || d->ownerFullName.isEmpty())
        return QString();

    const bool yourSystem = qnClientCoreSettings->cloudLogin() == d->ownerEmail;

    return yourSystem ? tr("Your system")
                      : tr("%1's system").arg(d->ownerFullName);
}

QnCloudSystemInformationWatcherPrivate::QnCloudSystemInformationWatcherPrivate(
        QnCloudSystemInformationWatcher* parent) :
    q_ptr(parent)
{
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::currentSystemChanged,
            this, &QnCloudSystemInformationWatcherPrivate::updateInformation);
}

void QnCloudSystemInformationWatcherPrivate::updateInformation(const QnCloudSystem& system)
{
    Q_Q(QnCloudSystemInformationWatcher);

    if (systemId != system.id)
    {
        systemId = system.id;
        emit q->systemIdChanged();
    }

    if (ownerEmail != system.ownerAccountEmail)
    {
        ownerEmail = system.ownerAccountEmail;
        emit q->ownerEmailChanged();
    }

    if (ownerFullName != system.ownerFullName)
    {
        ownerFullName = system.ownerFullName;
        emit q->ownerFullNameChanged();
    }
}

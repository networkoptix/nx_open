// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_information_watcher.h"

#include <client_core/client_core_settings.h>
#include <common/common_module.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/common/system_settings.h>
#include <watchers/cloud_status_watcher.h>

class QnCloudSystemInformationWatcherPrivate:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    QnCloudSystemInformationWatcher* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudSystemInformationWatcher)

public:
    QString systemId;
    QString ownerEmail;
    QString ownerFullName;
    QnCloudSystem currentSystem;

public:
    QnCloudSystemInformationWatcherPrivate(QnCloudSystemInformationWatcher* parent);
    void updateInformation(const QnCloudSystem& system);

    void updateCurrentSystem()
    {
        const auto systemId = globalSettings()->cloudSystemId();
        const auto cloudSystems = qnCloudStatusWatcher->cloudSystems();

        const auto it = std::find_if(
            cloudSystems.begin(), cloudSystems.end(),
            [systemId](const QnCloudSystem& system) { return systemId == system.cloudId; });

        if (it == cloudSystems.end())
            return;

        if (!it->visuallyEqual(currentSystem))
        {
            currentSystem = *it;
            updateInformation(currentSystem);
        }
    }
};

QnCloudSystemInformationWatcher::QnCloudSystemInformationWatcher(QObject* parent) :
    base_type(parent),
    d_ptr(new QnCloudSystemInformationWatcherPrivate(this))
{
    connect(this, &QnCloudSystemInformationWatcher::ownerEmailChanged,
            this, &QnCloudSystemInformationWatcher::ownerDescriptionChanged);
    connect(this, &QnCloudSystemInformationWatcher::ownerFullNameChanged,
            this, &QnCloudSystemInformationWatcher::ownerDescriptionChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudLoginChanged, this,
        &QnCloudSystemInformationWatcher::ownerDescriptionChanged);
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

    const bool yourSystem = qnCloudStatusWatcher->cloudLogin() == d->ownerEmail;

    return yourSystem ? tr("Your System")
                      : tr("Owner: %1", "%1 is a user name").arg(d->ownerFullName);
}

QnCloudSystemInformationWatcherPrivate::QnCloudSystemInformationWatcherPrivate(
    QnCloudSystemInformationWatcher* parent)
    :
    q_ptr(parent)
{
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged,
        this, &QnCloudSystemInformationWatcherPrivate::updateCurrentSystem);
    connect(globalSettings(), &nx::vms::common::SystemSettings::cloudSettingsChanged,
        this, &QnCloudSystemInformationWatcherPrivate::updateCurrentSystem);
}

void QnCloudSystemInformationWatcherPrivate::updateInformation(const QnCloudSystem& system)
{
    Q_Q(QnCloudSystemInformationWatcher);

    if (systemId != system.cloudId)
    {
        systemId = system.cloudId;
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

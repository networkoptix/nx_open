// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_information_watcher.h"

#include <nx/vms/client/core/context_from_qml_handler.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/client/mobile/window_context_aware.h>
#include <nx/vms/common/system_settings.h>

using namespace nx::vms::client::core;

using nx::vms::common::SystemSettings;
using ContextAware = nx::vms::client::mobile::WindowContextAware;

class QnCloudSystemInformationWatcherPrivate: public QObject,
    public ContextAware,
    public ContextFromQmlHandler
{
    using base_type = ContextAware;

    QnCloudSystemInformationWatcher* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudSystemInformationWatcher)

public:
    QString systemId;
    QString ownerEmail;
    QString ownerFullName;
    QnCloudSystem currentSystem;
    QPointer<SystemContext> currentSystemContext;

    virtual void onContextReady() override
    {
        const auto context = windowContext();

        auto updateContext =
            [this]()
            {
                const auto newContext = system();
                if (currentSystemContext == newContext)
                    return;

                currentSystemContext = newContext;
                updateSystemInfo();

                if (currentSystemContext)
                {
                    const auto settings = currentSystemContext->globalSettings();
                    connect(settings, &SystemSettings::cloudSettingsChanged,
                        this, &QnCloudSystemInformationWatcherPrivate::updateSystemInfo);
                }
            };

        connect(context, &nx::vms::client::mobile::WindowContext::mainSystemContextChanged,
            this, updateContext);
    }

public:
    QnCloudSystemInformationWatcherPrivate(QnCloudSystemInformationWatcher* parent);
    void updateInformation(const QnCloudSystem& system);

    void updateSystemInfo()
    {
        const auto systemId = currentSystemContext
            ? currentSystemContext->globalSettings()->cloudSystemId()
            : QString{};

        const auto cloudSystems = qnCloudStatusWatcher->cloudSystems();
        const auto it = std::find_if(
            cloudSystems.begin(), cloudSystems.end(),
            [systemId](const QnCloudSystem& system) { return systemId == system.cloudId; });

        if (it == cloudSystems.end())
            updateInformation({});
        else if (!it->visuallyEqual(currentSystem))
            updateInformation(currentSystem = *it);
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
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudLoginChanged, this,
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

    return yourSystem
        ? tr("Your Site")
        : tr("Owner: %1", "%1 is a user name").arg(d->ownerFullName);
}

QnCloudSystemInformationWatcherPrivate::QnCloudSystemInformationWatcherPrivate(
    QnCloudSystemInformationWatcher* parent)
    :
    QObject(),
    base_type(nx::vms::client::mobile::WindowContext::fromQmlContext(parent)),
    q_ptr(parent)
{
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudSystemsChanged,
        this, &QnCloudSystemInformationWatcherPrivate::updateSystemInfo);
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

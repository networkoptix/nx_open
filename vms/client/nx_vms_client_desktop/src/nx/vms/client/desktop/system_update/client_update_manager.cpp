// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_update_manager.h"

#include <chrono>
#include <future>

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/vms/api/data/client_update_settings.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/update/tools.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/synctime.h>

#include "client_update_tool.h"
#include "requests.h"
#include "update_verification.h"

using namespace std::chrono;
using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

milliseconds kClientUpdateRolloutPeriod = 4h;

QRandomGenerator64 getRandomGenerator(
    const QnUuid& localSystemId, const nx::utils::SoftwareVersion& version)
{
    // We need a seeded random generator to ensure that all clients of one system pick the same
    // date. So we seed it with the localSystemId. The update version is also fed to the generator
    // to avoid cases when clients in the system pick the same date shift for every update.
    QByteArray seed = localSystemId.toRfc4122();

    int n = version.major;
    seed.reserve(seed.size() + 4 * sizeof(n));
    seed.append((char*) &n, sizeof(n));
    n = version.minor;
    seed.append((char*) &n, sizeof(n));
    n = version.bugfix;
    seed.append((char*) &n, sizeof(n));
    n = version.build;
    seed.append((char*) &n, sizeof(n));

    return QRandomGenerator64((const quint32*) seed.data(), seed.size() / sizeof(quint32));
}

} // namespace

class ClientUpdateManager::Private: public QObject, public QnWorkbenchContextAware
{
    Q_DECLARE_TR_FUNCTIONS(nx::vms::update::desktop::ClientUpdateManager)

public:
    Private(ClientUpdateManager* q);

    void checkForUpdate();
    void checkUpdateTime();
    void startUpdate();
    void notifyUserAboutSuccessfulInstallation();
    void hideSuccessfulInstallationNotification();
    void notifyUserAboutClientAutoUpdate();
    void disableClientAutoUpdateNofitication();
    void hideClientAutoUpdateNofitication();
    void showErrorNotification(const QString& message);
    void hideErrorNotification();

    void setEnabled(bool enabled);
    void setGlobalPlannedUpdate(
        const nx::utils::SoftwareVersion& version, milliseconds updateDateTime);
    bool isGlobalUpdateDateUsed() const;
    void planUpdate();

    void handleConnectionStateChanged(QnConnectionState state);
    void handleClientUpdateSettingsChanged();
    void handleUpdateContents(UpdateContents newContents);
    void handleUpdateStateChanged(ClientUpdateTool::State state);

    nx::vms::api::ClientUpdateSettings globalClientUpdateSettings() const;
    void setGlobalClientUpdateSettings(const nx::vms::api::ClientUpdateSettings& settings);
    void cancelRequest();

public:
    ClientUpdateManager* q = nullptr;
    CommandActionPtr restartAction;
    CommandActionPtr settingsAction;
    ClientUpdateTool* clientUpdateTool = nullptr;
    workbench::LocalNotificationsManager* notificationsManager = nullptr;
    QTimer* checkForUpdateTimer = nullptr;
    QTimer* checkUpdateTimeTimer = nullptr;
    std::future<void> updateContentsFuture;
    bool connected = false;

    bool enabled = false;
    nx::utils::SoftwareVersion globalPlannedVersion;
    milliseconds globalPlannedUpdateDateTime{0};

    milliseconds generatedUpdateDateTime{0};
    milliseconds updateDateTimeShift{0};

    UpdateContents updateContents;
    QString updateVerificationError;
    QnUuid installationNotificationId;
    QnUuid autoUpdateFeatureNotificationId;
    QnUuid errorNotificationId;
    rest::Handle currentRequest = 0;
};

ClientUpdateManager::Private::Private(ClientUpdateManager* q):
    QnWorkbenchContextAware(q),
    q(q),
    restartAction(new QAction(tr("Restart Client"))),
    settingsAction(new QAction(tr("Updates Settings"))),
    clientUpdateTool(new ClientUpdateTool(systemContext(), this)),
    notificationsManager(windowContext()->localNotificationsManager()),
    checkForUpdateTimer(new QTimer(this)),
    checkUpdateTimeTimer(new QTimer(this))
{
    NX_CRITICAL(notificationsManager);

    const QnClientConnectionStatus* connectionStatus =
        qnClientMessageProcessor->connectionStatus();
    connect(connectionStatus,
        &QnClientConnectionStatus::stateChanged,
        this,
        &Private::handleConnectionStateChanged);

    connect(systemSettings(),
        &SystemSettings::clientUpdateSettingsChanged,
        this,
        &Private::handleClientUpdateSettingsChanged);

    checkForUpdateTimer->setInterval(24h);
    checkForUpdateTimer->callOnTimeout(this, &Private::checkForUpdate);

    checkUpdateTimeTimer->setInterval(10min);
    checkUpdateTimeTimer->callOnTimeout(this, &Private::checkUpdateTime);

    connect(clientUpdateTool,
        &ClientUpdateTool::updateStateChanged,
        this,
        &Private::handleUpdateStateChanged);

    connect(restartAction.data(),
        &QAction::triggered,
        this,
        [this]()
        {
            std::optional<nx::vms::client::core::LogonData> logonData;
            if (const auto connection = this->connection())
                logonData = connection->createLogonData();
            clientUpdateTool->restartClient(logonData);
        });

    connect(settingsAction.data(),
        &QAction::triggered,
        this,
        [this]()
        {
            disableClientAutoUpdateNofitication();
            menu()->trigger(menu::AdvancedUpdateSettingsAction);
        });

    connect(notificationsManager,
        &workbench::LocalNotificationsManager::cancelRequested,
        this,
        [this](const QnUuid& notificationId)
        {
            if (notificationId == installationNotificationId)
                hideSuccessfulInstallationNotification();
            else if (notificationId == autoUpdateFeatureNotificationId)
                disableClientAutoUpdateNofitication();
        });

    connect(notificationsManager,
        &workbench::LocalNotificationsManager::interactionRequested,
        this,
        [this](const QnUuid& notificationId)
        {
            if (!systemContext()->accessController()->hasPowerUserPermissions())
                return;

            if (notificationId == errorNotificationId
                || notificationId == autoUpdateFeatureNotificationId
                || notificationId == installationNotificationId)
            {
                disableClientAutoUpdateNofitication();
                menu()->trigger(menu::AdvancedUpdateSettingsAction);
            }
        });
}

void ClientUpdateManager::Private::checkForUpdate()
{
    NX_DEBUG(this, "Starting a check for update.");

    if (checkForUpdateTimer->isActive())
        checkForUpdateTimer->start();

    if (updateContentsFuture.valid())
    {
        NX_VERBOSE(this, "Previous check for update has not finished yet. Skipping...");
        return;
    }

    switch (const auto state = clientUpdateTool->getState())
    {
        case ClientUpdateTool::State::initial:
        case ClientUpdateTool::State::complete:
        case ClientUpdateTool::State::error:
            break;
        default:
            NX_VERBOSE(this,
                "Previous update installation has not finished yet (state = %1). Skipping...",
                ClientUpdateTool::toString(state));
            return;
    }

    updateContentsFuture = std::async(std::launch::async,
        [this]()
        {
            const UpdateContents updateContents =
                system_update::getUpdateContents(connectedServerApi(),
                    nx::vms::common::update::releaseListUrl(systemContext()),
                    update::LatestDesktopClientVersionParams{
                        nx::utils::SoftwareVersion(appContext()->version()),
                        nx::build_info::publicationType(),
                        api::protocolVersion()},
                    false);

            NX_VERBOSE(this, "getUpdateContents() request has finished.");
            QMetaObject::invokeMethod(
                this,
                [this, updateContents]() { handleUpdateContents(updateContents); },
                Qt::QueuedConnection);
        });
}

void ClientUpdateManager::Private::checkUpdateTime()
{
    if (updateContents.isEmpty() || !updateContents.clientPackage)
        return;

    const auto now = qnSyncTime->currentDateTime();

    if (const int day = now.date().dayOfWeek(); day >= Qt::Thursday && day <= Qt::Saturday)
    {
        NX_VERBOSE(this,
            "Updates are prohibited on Thursdays, Fridays and Saturdays. "
                "Skipping the update time check. Current date: %1", now);
        return;
    }

    const auto updateDateTime = q->plannedUpdateDate();

    NX_VERBOSE(this,
        "Checking update time. Update date: %1, now: %2, shift: %3 min. "
            "Used global update date: %4",
        updateDateTime,
        now,
        duration_cast<minutes>(updateDateTimeShift).count(),
        isGlobalUpdateDateUsed());

    if (now < updateDateTime.addMSecs(duration_cast<milliseconds>(updateDateTimeShift).count()))
        return;

    NX_INFO(this, "It's time for update.");
    startUpdate();
}

void ClientUpdateManager::Private::startUpdate()
{
    NX_VERBOSE(this, "Starting update to %1", updateContents.info.version);
    checkUpdateTimeTimer->stop();
    hideErrorNotification();
    clientUpdateTool->setUpdateTarget(updateContents);
}

void ClientUpdateManager::Private::notifyUserAboutSuccessfulInstallation()
{
    hideSuccessfulInstallationNotification();

    NX_VERBOSE(this,
        "Showing a notification about successful update to %1.",
        updateContents.info.version);

    const auto versionUrl = QString("<a href=\"%2\">%1</a>").arg(
        updateContents.info.version.toString(nx::utils::SoftwareVersion::Format::bugfix),
        q->releaseNotesUrl().toString());
    installationNotificationId = notificationsManager->add(tr("Client update is installed"),
        tr("Client is updated to version %1. Restart %2 to finish update.",
            "%1 is a version like '4.3.0', %2 is a VMS name like 'Nx Witness'")
            .arg(versionUrl, branding::vmsName()),
        /*cancellable*/ true);
    notificationsManager->setIcon(
        installationNotificationId, qnSkin->pixmap("events/update_complete.svg"));
    notificationsManager->setProgress(
        installationNotificationId, ProgressState::completed);
    notificationsManager->setAction(installationNotificationId, restartAction);
}

void ClientUpdateManager::Private::hideSuccessfulInstallationNotification()
{
    if (!installationNotificationId.isNull())
    {
        NX_VERBOSE(this, "Hide notification about successful installation.");
        notificationsManager->remove(installationNotificationId);
    }
    installationNotificationId = {};
}

void ClientUpdateManager::Private::notifyUserAboutClientAutoUpdate()
{
    if (!autoUpdateFeatureNotificationId.isNull())
        return;

    if (!systemContext()->accessController()->hasPowerUserPermissions())
        return;

    NX_VERBOSE(this, "Showing a notification about Client-only auto-update feature.");

    autoUpdateFeatureNotificationId = notificationsManager->add(tr("Automatic client updates"),
        tr("New client-only updates will be installed automatically. "
           "You can change this in the settings."),
        /*cancellable*/ true);
    notificationsManager->setIcon(
        autoUpdateFeatureNotificationId, qnSkin->pixmap("events/update_auto.svg"));
    notificationsManager->setProgress(
        autoUpdateFeatureNotificationId, ProgressState::completed);
    notificationsManager->setAction(autoUpdateFeatureNotificationId, settingsAction);
}

void ClientUpdateManager::Private::disableClientAutoUpdateNofitication()
{
    NX_VERBOSE(this, "Disabling notifications about Client-only auto-update feature.");

    hideClientAutoUpdateNofitication();

    auto settings = globalClientUpdateSettings();
    settings.showFeatureInformer = false;
    setGlobalClientUpdateSettings(settings);
}

void ClientUpdateManager::Private::hideClientAutoUpdateNofitication()
{
    if (!autoUpdateFeatureNotificationId.isNull())
    {
        NX_VERBOSE(this, "Hide notification about client-only auto updates.");
        notificationsManager->remove(autoUpdateFeatureNotificationId);
    }
    autoUpdateFeatureNotificationId = {};
}

void ClientUpdateManager::Private::showErrorNotification(const QString& message)
{
    NX_VERBOSE(this, "Showing error notification: \"%1\".", message);

    if (errorNotificationId.isNull())
    {
        errorNotificationId = notificationsManager->add(message);
        notificationsManager->setProgress(
            errorNotificationId, ProgressState::failed);
    }
    else
    {
        notificationsManager->setTitle(errorNotificationId, message);
    }

    notificationsManager->setIcon(errorNotificationId, qnSkin->pixmap("events/alert_yellow.png"));
}

void ClientUpdateManager::Private::hideErrorNotification()
{
    if (!errorNotificationId.isNull())
    {
        NX_VERBOSE(this, "Hide update error notification.");
        notificationsManager->remove(errorNotificationId);
    }
    errorNotificationId = {};
}

void ClientUpdateManager::Private::setEnabled(bool enabled)
{
    if (this->enabled == enabled)
        return;

    this->enabled = enabled;
    emit q->clientUpdateEnabledChanged();

    if (connected && enabled != globalClientUpdateSettings().enabled)
    {
        nx::vms::api::ClientUpdateSettings settings;
        settings.enabled = enabled;
        settings.showFeatureInformer = false;
        settings.updateEnabledTimestampMs =
            enabled ? qnSyncTime->value() : 0ms;
        setGlobalClientUpdateSettings(settings);
    }

    if (enabled)
    {
        NX_INFO(this, "Client-only update is enabled. Starting update timers...");
        checkForUpdateTimer->start();
        checkUpdateTimeTimer->start();
        checkForUpdate();
    }
    else
    {
        NX_INFO(this, "Client-only update is disabled. Stopping update timers...");
        checkForUpdateTimer->stop();
        checkUpdateTimeTimer->stop();
        clientUpdateTool->resetState();
        hideClientAutoUpdateNofitication();
        hideSuccessfulInstallationNotification();
        hideErrorNotification();
        updateContents = {};
        updateVerificationError.clear();
        emit q->errorMessageChanged();
    }
}

void ClientUpdateManager::Private::setGlobalPlannedUpdate(
    const nx::utils::SoftwareVersion& version, milliseconds updateDateTime)
{
    globalPlannedVersion = version;
    globalPlannedUpdateDateTime = updateDateTime;
    emit q->plannedUpdateDateChanged();

    if (version.isNull())
        return;

    if (updateContents.isEmpty())
        checkForUpdate();
    else if (updateContents.info.version == version)
        checkUpdateTime();
}

bool ClientUpdateManager::Private::isGlobalUpdateDateUsed() const
{
    return !ini().startClientOnlyUpdateImmediately
        && globalPlannedVersion == updateContents.info.version;
}

void ClientUpdateManager::Private::planUpdate()
{
    if (ini().startClientOnlyUpdateImmediately)
    {
        generatedUpdateDateTime = qnSyncTime->value();
        updateDateTimeShift = 0ms;
        emit q->plannedUpdateDateChanged();
        return;
    }

    milliseconds kUpdatesProhibitedSinceActivationPeriod = 24h;
    const auto updateAllowedSince = globalClientUpdateSettings().updateEnabledTimestampMs
        + kUpdatesProhibitedSinceActivationPeriod;

    const UpdateDate plannedDate = calculateUpdateDate(
        qnSyncTime->value(),
        updateContents.info,
        systemSettings()->localSystemId(),
        updateAllowedSince);

    generatedUpdateDateTime = plannedDate.date;
    updateDateTimeShift = plannedDate.shift;
    emit q->plannedUpdateDateChanged();
}

void ClientUpdateManager::Private::handleConnectionStateChanged(QnConnectionState state)
{
    if (state == QnConnectionState::Disconnected)
    {
        connected = false;
        setEnabled(false);
        return;
    }

    if (!connected && state != QnConnectionState::Ready)
        return;

    connected = true;

    handleClientUpdateSettingsChanged();
}

void ClientUpdateManager::Private::handleClientUpdateSettingsChanged()
{
    if (!connected)
        return;

    const auto settings = globalClientUpdateSettings();

    NX_VERBOSE(this,
        "Global client update settings chaged. Enabled: %1, pending version: %2, planned date: %3",
        settings.enabled,
        settings.pendingVersion,
        QDateTime::fromMSecsSinceEpoch(settings.plannedInstallationDateMs.count()));

    setEnabled(settings.enabled);
    setGlobalPlannedUpdate(settings.pendingVersion, settings.plannedInstallationDateMs);

    if (settings.enabled && settings.showFeatureInformer)
        notifyUserAboutClientAutoUpdate();
}

void ClientUpdateManager::Private::handleUpdateContents(UpdateContents newContents)
{
    if (!connected)
        return;

    updateContentsFuture.get();

    bool verificationPassed = false;
    if (!newContents.isEmpty())
    {
        ClientVerificationData clientData;
        clientData.clientId = QnUuid::createUuid(); //< Any unique ID is OK.
        clientData.fillDefault();
        VerificationOptions options;
        options.compatibilityMode = true;
        options.systemContext = systemContext();
        options.downloadAllPackages = false;

        verificationPassed =
            verifyUpdateContents(newContents, /*servers=*/{}, clientData, options);
    }

    if (!verificationPassed)
    {
        bool errorAffectsAnyClient = false;
        switch (newContents.error)
        {
            case common::update::InformationError::noError:
            case common::update::InformationError::noNewVersion:
                updateVerificationError.clear();
                errorAffectsAnyClient = true;
                break;

            case common::update::InformationError::emptyPackagesUrls:
            case common::update::InformationError::incompatibleCloudHost:
            case common::update::InformationError::incompatibleParser:
            case common::update::InformationError::incompatibleVersion:
            case common::update::InformationError::notFoundError:
            case common::update::InformationError::serverConnectionError:
                updateVerificationError = tr("Cannot check for update.");
                errorAffectsAnyClient = true;
                break;

            case common::update::InformationError::httpError:
            case common::update::InformationError::networkError:
            case common::update::InformationError::jsonError:
                updateVerificationError = tr("Cannot check Client update availability. "
                    "Check your internet connection and try again.");
                errorAffectsAnyClient = true;
                break;

            case common::update::InformationError::brokenPackageError:
                updateVerificationError = tr("Client update package is broken.");
                break;

            case common::update::InformationError::missingPackageError:
                updateVerificationError =
                    tr("Client update package is missing for current platform.");
                break;
        }
        NX_DEBUG(this, "UpdateContents verification failed: %1.", newContents.error);

        emit q->errorMessageChanged();

        if (errorAffectsAnyClient)
            return;
    }
    else
    {
        updateVerificationError.clear();
        emit q->errorMessageChanged();
    }

    if (!updateVerificationError.isEmpty())
        showErrorNotification(updateVerificationError);
    else
        hideErrorNotification();

    const bool deliveryInfoChanged =
        (updateContents.getUpdateDeliveryInfo() != newContents.getUpdateDeliveryInfo());

    updateContents = newContents;

    if (deliveryInfoChanged)
    {
        emit q->pendingVersionChanged();
        emit q->installationNeededChanged();
        planUpdate();
    }

    checkUpdateTime();
}

void ClientUpdateManager::Private::handleUpdateStateChanged(ClientUpdateTool::State state)
{
    if (!connected)
        return;

    NX_VERBOSE(this, "ClientUpdateTool state changed: %1", ClientUpdateTool::toString(state));

    hideErrorNotification();

    switch (state)
    {
        case ClientUpdateTool::State::readyInstall:
            NX_VERBOSE(
                this, "Update %1 is ready. Starting installation.", updateContents.info.version);
            clientUpdateTool->installUpdateAsync();
            break;

        case ClientUpdateTool::State::complete:
        case ClientUpdateTool::State::readyRestart:
            NX_DEBUG(this, "Update %1 has finished successfully.", updateContents.info.version);
            notifyUserAboutSuccessfulInstallation();
            emit q->installationNeededChanged();
            break;

        case ClientUpdateTool::State::error:
        {
            const QString errorMessage =
                ClientUpdateTool::errorString(clientUpdateTool->getError());
            showErrorNotification(errorMessage);
            NX_VERBOSE(this, "Update installation failed: %1", errorMessage);
        }
        default:
            break;
    }

    emit q->errorMessageChanged();
}

nx::vms::api::ClientUpdateSettings ClientUpdateManager::Private::globalClientUpdateSettings() const
{
    return systemSettings()->clientUpdateSettings();
}

void ClientUpdateManager::Private::setGlobalClientUpdateSettings(
    const nx::vms::api::ClientUpdateSettings& settings)
{
    auto callback =
        [this](bool /*success*/, rest::Handle requestId, rest::ServerConnection::ErrorOrEmpty)
        {
            NX_ASSERT(requestId == currentRequest || currentRequest == 0);
            currentRequest = 0;
        };

    currentRequest = systemContext()->connectedServerApi()->patchSystemSettings(
        systemContext()->getSessionTokenHelper(),
        api::SaveableSystemSettings{.clientUpdateSettings = settings},
        callback,
        this);
}

void ClientUpdateManager::Private::cancelRequest()
{
    if (auto api = connectedServerApi(); api && currentRequest > 0)
        api->cancelRequest(currentRequest);
    currentRequest = 0;
}

//-------------------------------------------------------------------------------------------------

ClientUpdateManager::ClientUpdateManager(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    qmlRegisterUncreatableType<ClientUpdateManager>("nx.vms.client.desktop",
        1,
        0,
        "ClientUpdateManager",
        "ClientUpdateManager can be created from C++ code only.");
}

ClientUpdateManager::~ClientUpdateManager()
{
    d->cancelRequest();
}

bool ClientUpdateManager::clientUpdateEnabled() const
{
    return d->enabled;
}

void ClientUpdateManager::setClientUpdateEnabled(bool enabled)
{
    d->setEnabled(enabled);
}

nx::utils::SoftwareVersion ClientUpdateManager::pendingVersion() const
{
    return d->updateContents.info.version;
}

QDateTime ClientUpdateManager::plannedUpdateDate() const
{
    const auto updateDateTime = d->isGlobalUpdateDateUsed()
        ? d->globalPlannedUpdateDateTime
        : d->generatedUpdateDateTime;

    return QDateTime::fromMSecsSinceEpoch(updateDateTime.count());
}

bool ClientUpdateManager::installationNeeded() const
{
    return !d->updateContents.isEmpty()
        && !d->clientUpdateTool->isVersionInstalled(d->updateContents.info.version);
}

QString ClientUpdateManager::errorMessage() const
{
    if (!d->updateVerificationError.isEmpty())
        return d->updateVerificationError;

    if (d->clientUpdateTool->getState() == ClientUpdateTool::State::error)
        return d->clientUpdateTool->errorString(d->clientUpdateTool->getError());

    return {};
}

void ClientUpdateManager::checkForUpdates()
{
    if (!d->enabled)
    {
        NX_WARNING(this, "Check for updates request ignored because client updates are disabled.");
        return;
    }

    d->checkForUpdate();
}

void ClientUpdateManager::speedUpCurrentUpdate()
{
    if (d->updateContents.info.version.isNull())
    {
        NX_DEBUG(this, "Cannot speed up current update, because current update version is null.");
        return;
    }

    auto settings = d->globalClientUpdateSettings();
    settings.pendingVersion = d->updateContents.info.version;
    settings.plannedInstallationDateMs = qnSyncTime->value();
    d->setGlobalClientUpdateSettings(settings);
}

bool ClientUpdateManager::isPlannedUpdateDatePassed() const
{
    return d->updateContents.info.isValid()
        && qnSyncTime->currentDateTime() >= plannedUpdateDate();
}

QUrl ClientUpdateManager::releaseNotesUrl() const
{
    return d->updateContents.info.releaseNotesUrl;
}

ClientUpdateManager::UpdateDate ClientUpdateManager::calculateUpdateDate(
    milliseconds currentDateTime,
    const common::update::Information& updateInfo,
    const QnUuid& localSystemId,
    milliseconds minimumAllowedUpdateDate)
{
    // IMPORTANT! This function must generate the same timestamp on all Clients in the system
    // independently on the Client timezone. Thus all QDateTime <-> std::chrono conversion calls
    // must be done with Qt::UTC timezone specification.

    const auto isSuitableDay =
        [](const QDate& day)
        {
            return day.dayOfWeek() <= Qt::Wednesday || day.dayOfWeek() == Qt::Sunday;
        };

    const auto qdt =
        [](milliseconds ts) { return QDateTime::fromMSecsSinceEpoch(ts.count(), Qt::UTC); };

    QRandomGenerator64 randomGenerator = getRandomGenerator(localSystemId, updateInfo.version);

    const milliseconds endOfDeliveryPeriod =
        updateInfo.releaseDate + updateInfo.releaseDeliveryDays * 24h;
    const milliseconds minDate =
        std::max(std::max(updateInfo.releaseDate, minimumAllowedUpdateDate), currentDateTime);
    const milliseconds maxDate = std::max(minDate, endOfDeliveryPeriod);

    NX_VERBOSE(typeid(ClientUpdateManager),
        "calculatePlannedUpdateDate(): Planning update date. "
            "Now: %1, release date: %2, delivery days: %3, minimum allowed date: %4, "
            "end of delivery: %5, min date: %6, max date: %7",
        qdt(currentDateTime),
        qdt(updateInfo.releaseDate),
        updateInfo.releaseDeliveryDays,
        qdt(minimumAllowedUpdateDate),
        qdt(endOfDeliveryPeriod),
        qdt(minDate),
        qdt(maxDate));

    QDate date;

    if (endOfDeliveryPeriod <= currentDateTime) //< Update rollout period has already passed.
    {
        date = qdt(minDate).date();
        if (!isSuitableDay(date))
            date = date.addDays(Qt::Sunday - date.dayOfWeek());

        NX_DEBUG(typeid(ClientUpdateManager),
            "calculatePlannedUpdateDate(): Update rollout period has already passed. "
                "Period ended at %1, now: %2",
            endOfDeliveryPeriod,
            qdt(currentDateTime));
    }
    else
    {
        date = qdt(milliseconds(nx::utils::random::number(
            randomGenerator, minDate.count(), maxDate.count()))).date();

        if (!isSuitableDay(date))
        {
            // Pick a random suitable day in the current or next week.

            constexpr int kChoiceDays = 6;

            std::vector<QDate> days;
            days.reserve(2 * kChoiceDays);

            for (int i = -kChoiceDays; i <= kChoiceDays; ++i)
            {
                const auto day = date.addDays(i);
                if (isSuitableDay(day)
                    && day.endOfDay(Qt::UTC).toMSecsSinceEpoch() > minDate.count()
                    && day.startOfDay(Qt::UTC).toMSecsSinceEpoch() < maxDate.count())
                {
                    days.push_back(day);
                }
            }

            // Pick the next suitable day if there are no suitable days in the rollout period.
            for (auto day = date.addDays(1); days.empty(); day = day.addDays(1))
            {
                if (isSuitableDay(day))
                    days.push_back(day);
            }

            date = days[randomGenerator.bounded((int) days.size())];
        }
    }

    NX_VERBOSE(typeid(ClientUpdateManager), "calculatePlannedUpdateDate(): Picked date: %1", date);

    UpdateDate plannedDate;

    const milliseconds minStartTime =
        std::max(minDate, milliseconds(date.startOfDay(Qt::UTC).toMSecsSinceEpoch()));
    const milliseconds maxStartTime = std::max(
        minStartTime, std::min(maxDate, milliseconds(date.endOfDay(Qt::UTC).toMSecsSinceEpoch())));
    const milliseconds maxRolloutStartTime =
        std::max(minStartTime, maxStartTime - kClientUpdateRolloutPeriod);
    const milliseconds localShiftWindow = maxStartTime - maxRolloutStartTime;

    NX_VERBOSE(typeid(ClientUpdateManager),
        "calculatePlannedUpdateDate(): min start time: %1, max start time: %2, "
            "max rollout start time: %3, local shift window: %4",
        qdt(minStartTime),
        qdt(maxStartTime),
        qdt(maxRolloutStartTime),
        localShiftWindow);

    // Pick a time point in the chosen day.
    plannedDate.date = milliseconds(nx::utils::random::number(
        randomGenerator, minStartTime.count(), maxRolloutStartTime.count()));
    plannedDate.shift = milliseconds(nx::utils::random::number<long>(0, localShiftWindow.count()));

    NX_DEBUG(typeid(ClientUpdateManager),
        "calculatePlannedUpdateDate(): Update planned to %1. Local shift: %2. "
            "This client will start installation at %3",
        qdt(plannedDate.date),
        plannedDate.shift,
        qdt(plannedDate.date + plannedDate.shift));

    return plannedDate;
}

} // namespace nx::vms::client::desktop

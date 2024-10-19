// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_manager.h"

#include <QtQml/QtQml>

#include <mobile_client/mobile_client_settings.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <utils/common/delayed.h>

#include "details/push_api_helper.h"
#include "details/push_ipc_data.h"
#include "details/push_notification_structures.h"
#include "details/push_permission_manager.h"
#include "details/push_platform_helpers.h"
#include "details/push_subscription_state_controller.h"

namespace nx::vms::client::mobile {

namespace {

using namespace nx::vms::client::mobile;
using namespace nx::vms::client::mobile::details;

OptionalLocalPushSettings settingsForUser(
    const PushNotificationManager* manager,
    const QString& user)
{
    if (user.isEmpty())
        return OptionalLocalPushSettings();

    const auto settings = qnSettings->userPushSettings();
    const auto it = settings.find(user);

    if (it == settings.cend())
        NX_DEBUG(manager, "Loading empty settings for %1", user);
    else
        NX_DEBUG(manager, "Loading settings for %1", user);

    return it == settings.cend()
        ? OptionalLocalPushSettings()
        : OptionalLocalPushSettings(it->second);
}

void updateUserSettings(
    const PushNotificationManager* manager,
    const QString& user,
    const OptionalLocalPushSettings& value)
{
    if (user.isEmpty())
        return;

    auto settings = qnSettings->userPushSettings();
    if (value)
    {
        NX_DEBUG(manager, "Storing settings for %1", user);
        settings[user] = *value;
    }
    else if (settings.erase(user))
    {
        NX_DEBUG(manager, "Removing settings for %1", user);
    }

    qnSettings->setUserPushSettings(settings);
    qnSettings->save();
}

SystemSet operator&(const SystemSet& lhs, const SystemSet& rhs)
{
    SystemSet result;
    std::set_intersection(
        lhs.cbegin(), lhs.cend(),
        rhs.cbegin(), rhs.cend(),
        std::inserter(result, result.end()));
    return result;
}

} // namespace

struct PushNotificationManager::Private
{
    PushNotificationManager* const q;

    PushPermissionManager permissionManager;
    PushSettingsRemoteController remotePushController;

    SystemSet currentSystems;

    Private(PushNotificationManager* q);
    void setTargetSystems(
        const SystemSet& systems,
        QJSValue callback);
    void showCannotSubscribeMessage() const;
    void emitSystemsMetricsChanges() const;
    void updateDigestPassword() const;
    void handleConfirmedSettingsChanged();
};

PushNotificationManager::Private::Private(PushNotificationManager* q):
    q(q)
{
    connect(&remotePushController, &PushSettingsRemoteController::confirmedSettingsChanged,
        q, [this]() { handleConfirmedSettingsChanged(); });

    using BaseProperty = nx::utils::property_storage::BaseProperty;
    connect(&core::appContext()->coreSettings()->digestCloudPassword, &BaseProperty::changed,
        q, [this]() { updateDigestPassword(); });
}

void PushNotificationManager::Private::setTargetSystems(
    const SystemSet& systems,
    QJSValue callback)
{
    if (remotePushController.credentials().authToken.empty())
        return;

    auto settings = remotePushController.confirmedSettings();
    if (!settings)
        settings = LocalPushSettings();

    settings->systems = systems;

    if (settings->enabled)
    {
        remotePushController.tryUpdateRemoteSettings(*settings,
            nx::utils::mutableGuarded(q,
            [this, callback](bool success) mutable
            {
                if (callback.isCallable())
                    callback.call(QJSValueList{success});

                if (success)
                    return;

                emit q->showPushErrorMessage(
                    tr("Cannot change push notifications settings"),
                    q->checkConnectionErrorText());
            }));
    }
    else
    {
        remotePushController.replaceConfirmedSettings(*settings);
        if (callback.isCallable())
            callback.call(QJSValueList{true});
    }
}

void PushNotificationManager::Private::handleConfirmedSettingsChanged()
{
    emit q->expertModeChanged();
    emit q->enabledCheckStateChanged();
    emit q->selectedSystemsChanged();
    emitSystemsMetricsChanges();

    const auto settings = remotePushController.confirmedSettings();
    if (!settings)
    {
        PushIpcData::clear();
        return;
    }

    const auto userName = remotePushController.credentials().username;
    updateUserSettings(q, QString::fromStdString(userName), settings);
    if (!settings->enabled)
    {
        PushIpcData::clear();
    }
    else if (!userName.empty())
    {
        PushIpcData::store(
            userName,
            core::appContext()->cloudStatusWatcher()->remoteConnectionCredentials().authToken.value,
            core::appContext()->coreSettings()->digestCloudPassword.value());
    }
}

void PushNotificationManager::Private::showCannotSubscribeMessage() const
{
    emit q->showPushErrorMessage(
        tr("Cannot enable push notifications"),
        q->checkConnectionErrorText());
}

void PushNotificationManager::Private::emitSystemsMetricsChanges() const
{
    emit q->systemsCountChanged();
    emit q->usedSystemsCountChanged();
}

void PushNotificationManager::Private::updateDigestPassword() const
{
    std::string user, cloudRefreshToken, password;
    PushIpcData::load(user, cloudRefreshToken, password);

    const auto updatedPassword = core::appContext()->coreSettings()->digestCloudPassword.value();
    if (password != updatedPassword)
        PushIpcData::store(user, cloudRefreshToken, updatedPassword);
}

//--------------------------------------------------------------------------------------------------

void PushNotificationManager::registerQmlType()
{
    qmlRegisterType<PushNotificationManager>("Nx.Mobile", 1, 0, "PushNotificationManager");
}

PushNotificationManager::PushNotificationManager(QObject* parent):
    base_type(parent),
    d(new Private{this})
{
    connect(&d->remotePushController, &PushSettingsRemoteController::userUpdateInProgressChanged,
        this, &PushNotificationManager::userUpdateInProgressChanged);
    connect(&d->remotePushController, &PushSettingsRemoteController::userUpdateInProgressChanged,
        this, &PushNotificationManager::enabledCheckStateChanged);
    connect(&d->remotePushController, &PushSettingsRemoteController::loggedInChanged,
        this, &PushNotificationManager::loggedInChanged);
    connect(&d->permissionManager, &PushPermissionManager::permissionChanged,
        this, &PushNotificationManager::hasOsPermissionChanged);
}

PushNotificationManager::~PushNotificationManager()
{
}

void PushNotificationManager::setCredentials(const nx::network::http::Credentials& value)
{
    if (d->remotePushController.credentials() == value)
        return;

    const nx::utils::ScopeGuard confirmedSettingsChangedGuard(
        [this]() { d->handleConfirmedSettingsChanged(); });

    const bool userLoggedOut = value.username.empty();;
    if (userLoggedOut)
    {
        d->remotePushController.logOut();
        return;
    }

    const auto settings = settingsForUser(this, QString::fromStdString(value.username));
    const auto explicitLoginCallback = nx::utils::guarded(this,
        [this](bool success)
        {
            if (success)
                return;

               // We turn off push notifications if we were not able to subscribe at first login.
            d->remotePushController.replaceConfirmedSettings(LocalPushSettings::makeLoggedOut());

            emit d->showCannotSubscribeMessage();
        });

    d->remotePushController.logIn(value, settings, explicitLoginCallback);
}

Qt::CheckState PushNotificationManager::enabledCheckState() const
{
    const auto& settings = d->remotePushController.confirmedSettings();
    if (!settings || userUpdateInProgress())
        return Qt::PartiallyChecked;

    return settings->enabled
        ? Qt::Checked
        : Qt::Unchecked;
}

void PushNotificationManager::setEnabled(bool value)
{
    if (d->remotePushController.credentials().authToken.empty())
        return;

    auto settings = d->remotePushController.confirmedSettings();
    const bool currentValue = !settings || settings->enabled;
    if (currentValue == value)
        return;

    settings->enabled = value;
    d->remotePushController.tryUpdateRemoteSettings(*settings,
        nx::utils::guarded(this,
        [this](bool success)
        {
            if (!success)
                d->showCannotSubscribeMessage();
        }));
}

bool PushNotificationManager::expertMode() const
{
    if (d->remotePushController.credentials().authToken.empty())
        return false;

    const auto& settings = d->remotePushController.confirmedSettings();
    return settings && isExpertMode(settings->systems);
}

void PushNotificationManager::setSimpleMode(QJSValue callback)
{
    d->setTargetSystems(allSystemsModeValue(), callback);
}

void PushNotificationManager::setExpertMode(
    const QStringList& systems,
    QJSValue callback)
{
    d->setTargetSystems(SystemSet(systems.cbegin(), systems.cend()), callback);
}

bool PushNotificationManager::userUpdateInProgress() const
{
    return d->remotePushController.userUpdateInProgress();
}

bool PushNotificationManager::loggedIn() const
{
    return d->remotePushController.loggedIn();
}

QStringList PushNotificationManager::selectedSystems() const
{
    const auto& settings = d->remotePushController.confirmedSettings();
    return settings
        ? QStringList(settings->systems.cbegin(), settings->systems.cend())
        : QStringList();
}

bool PushNotificationManager::hasOsPermission() const
{
    return d->permissionManager.permission() == PushPermission::authorized;
}

void PushNotificationManager::showOsPushSettings()
{
    details::showOsPushSettingsScreen();
}

int PushNotificationManager::systemsCount() const
{
    return d->currentSystems.size();
}

void PushNotificationManager::setSystems(const QStringList& value)
{
    const auto targetSystems = SystemSet(value.cbegin(), value.cend());
    if (d->currentSystems == targetSystems)
        return;

    d->currentSystems = targetSystems;

    d->emitSystemsMetricsChanges();
}

int PushNotificationManager::usedSystemsCount() const
{
    const auto selected =
        [this]()
        {
            const auto& settings = d->remotePushController.confirmedSettings();
            return settings
                ? SystemSet(settings->systems.cbegin(), settings->systems.cend())
                : SystemSet();
        }();

    return (selected & d->currentSystems).size();
}

QString PushNotificationManager::checkConnectionErrorText() const
{
    return tr("Please check your internet connection");
}

} // namespace nx::vms::client::mobile

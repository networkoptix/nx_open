// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_settings_widget.h"

#include <chrono>
#include <memory>
#include <optional>
#include <unordered_set>

#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QVBoxLayout>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_administration/globals/results_reporter.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/time/formatter.h>
#include <ui/dialogs/common/message_box.h>
#include <utils/common/synctime.h>

#include "../models/ldap_filters_model.h"

namespace {

static const QUrl kLdapSettingsQmlComponentUrl("qrc:/qml/Nx/Dialogs/Ldap/Tabs/LdapSettings.qml");
static const auto kStatusUpdateInterval = std::chrono::seconds(10);

} // namespace

namespace nx::vms::client::desktop {

using LdapState = QmlDialogWithState<LdapSettings>;

struct LdapSettingsWidget::Private
{
    LdapSettingsWidget* const q;

    QQuickWidget* const quickWidget;

    QmlProperty<QObject*> self;
    QmlProperty<TestState> testState;
    QmlProperty<QString> testMessage;

    QmlProperty<QString> lastSync;
    QmlProperty<int> userCount;
    QmlProperty<int> groupCount;

    QmlProperty<bool> checkingStatus;
    QmlProperty<bool> online;
    QmlProperty<bool> syncIsRunning;
    QmlProperty<bool> syncRequested;
    QmlProperty<bool> hideEmptyLdapWarning;

    QmlProperty<bool> modified;

    const QmlProperty<bool> hasConfig;

    nx::vms::api::LdapSettings initialSettings;

    rest::Handle currentHandle = 0;
    rest::Handle statusHandle = 0;

    QTimer statusUpdateTimer;

    Private(LdapSettingsWidget* parent):
        q(parent),
        quickWidget(new QQuickWidget(qnClientCoreModule->mainQmlEngine(), parent)),
        self(quickWidget, "self"),
        testState(quickWidget, "testState"),
        testMessage(quickWidget, "testMessage"),
        lastSync(quickWidget, "lastSync"),
        userCount(quickWidget, "userCount"),
        groupCount(quickWidget, "groupCount"),
        checkingStatus(quickWidget, "checkingStatus"),
        online(quickWidget, "online"),
        syncIsRunning(quickWidget, "syncIsRunning"),
        syncRequested(quickWidget, "syncRequested"),
        hideEmptyLdapWarning(quickWidget, "hideEmptyLdapWarning"),
        modified(quickWidget, "modified"),
        hasConfig(quickWidget, "hasConfig")
    {
        connect(quickWidget, &QQuickWidget::statusChanged,
            [this](QQuickWidget::Status status)
            {
                switch (status)
                {
                    case QQuickWidget::Error:
                        for (const auto& error: quickWidget->errors())
                            NX_ERROR(this, "QML Error: %1", error.toString());
                        return;

                    case QQuickWidget::Ready:
                        LdapState::subscribeToChanges(
                            quickWidget->rootObject(),
                            q, &LdapSettingsWidget::stateChanged);
                        return;

                    default:
                        return;
                };
            });

        // Export TestState enum to QML.
        qmlRegisterUncreatableType<LdapSettingsWidget>(
            "nx.vms.client.desktop", 1, 0, "LdapSettings",
            "Cannot create an instance of LdapSettings.");

        quickWidget->setMinimumSize({750, 450});
        quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        quickWidget->setSource(kLdapSettingsQmlComponentUrl);

        QObject::connect(q, &LdapSettingsWidget::stateChanged,
            [this]
            {
                const auto currentSettings = getState();
                modified = currentSettings != initialSettings;
            });

        modified.connectNotifySignal(q, [this]{ emit q->hasChangesChanged(); });
        self = parent;

        statusUpdateTimer.setInterval(kStatusUpdateInterval);
        statusUpdateTimer.setSingleShot(false);

        q->connect(&statusUpdateTimer, &QTimer::timeout,
            [this]
            {
                if (currentHandle <= 0)
                    q->loadSettings(/*force*/ false);
            });
    }

    void setState(const nx::vms::api::LdapSettings& settings)
    {
        LdapSettings state;

        state.uri = settings.uri.toString();
        state.adminDn = settings.adminDn;
        state.password = settings.adminPassword.value_or("");
        state.syncTimeoutS = settings.continuousSyncIntervalS.count();
        for (const auto& filter: settings.filters)
        {
            state.filters.push_back({
                .name = filter.name,
                .base = filter.base,
                .filter = filter.filter
            });
        }
        state.continuousSync =
            settings.continuousSync == nx::vms::api::LdapSettings::Sync::usersAndGroups;
        state.continuousSyncEditable =
            settings.continuousSync != nx::vms::api::LdapSettings::Sync::disabled;

        state.loginAttribute = settings.loginAttribute;
        state.groupObjectClass = settings.groupObjectClass;
        state.memberAttribute = settings.memberAttribute;

        state.preferredSyncServer = settings.preferredMasterSyncServer;

        LdapState::stateToObject(state, quickWidget->rootObject());
    }

    nx::vms::api::LdapSettings getState() const
    {
        nx::vms::api::LdapSettings settings = initialSettings;

        const auto state = LdapState::objectToState(quickWidget->rootObject());

        settings.uri = nx::utils::Url::fromUserInput(state.uri);
        settings.adminDn = state.adminDn;
        settings.adminPassword = state.password.isEmpty()
            ? std::nullopt
            : std::optional<QString>(state.password);
        settings.continuousSyncIntervalS = std::chrono::seconds(state.syncTimeoutS);
        settings.filters.clear();
        for (const auto& filter: state.filters)
        {
            settings.filters.push_back({
                .name = filter.name,
                .base = filter.base,
                .filter = filter.filter
            });
        }

        if (state.continuousSyncEditable)
        {
            settings.continuousSync = state.continuousSync
                ? nx::vms::api::LdapSettings::Sync::usersAndGroups
                : nx::vms::api::LdapSettings::Sync::groupsOnly;
        }
        else
        {
            settings.continuousSync = nx::vms::api::LdapSettings::Sync::disabled;
        }

        settings.loginAttribute = state.loginAttribute;
        settings.groupObjectClass = state.groupObjectClass;
        settings.memberAttribute = state.memberAttribute;

        settings.preferredMasterSyncServer = state.preferredSyncServer;

        return settings;
    }

    void updateUserAndGroupCount()
    {
        userCount = getLdapUserCount();
        groupCount = getLdapGroupCount();
    }

    int getLdapUserCount() const
    {
        const auto ldapUsers = q->systemContext()->resourcePool()->getResources<QnUserResource>(
            [](const auto& user) { return user->isLdap(); });

        return ldapUsers.size();
    }

    int getLdapGroupCount() const
    {
        const auto groups = q->systemContext()->userGroupManager()->groups();
        return std::count_if(groups.begin(), groups.end(),
            [](const auto& group)
            {
                return group.type == nx::vms::api::UserType::ldap
                    && group.id != nx::vms::api::kDefaultLdapGroupId;
            });
    }

    bool isAdmin() const
    {
        const auto systemContext = q->systemContext();

        return systemContext
            && systemContext->resourceAccessManager()->hasPowerUserPermissions(
                systemContext->userWatcher()->user());
    }
};

LdapSettingsWidget::LdapSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    setMinimumHeight(520);

    auto boxLayout = new QVBoxLayout(this);
    boxLayout->setContentsMargins(0, 0, 0, 0);
    boxLayout->addWidget(d->quickWidget);
}

LdapSettingsWidget::~LdapSettingsWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void LdapSettingsWidget::discardChanges()
{
    d->statusUpdateTimer.stop();

    if (d->currentHandle)
    {
        connectedServerApi()->cancelRequest(d->currentHandle);
        d->currentHandle = 0;
    }

    if (d->statusHandle)
    {
        connectedServerApi()->cancelRequest(d->statusHandle);
        d->statusHandle = 0;
    }

    d->setState(d->initialSettings);
}

void LdapSettingsWidget::checkStatus()
{
    const auto statusCallback = nx::utils::guarded(this,
        [this](
            bool success, int handle, rest::ErrorOrData<nx::vms::api::LdapStatus> errorOrData)
        {
            if (handle == d->statusHandle)
                d->statusHandle = 0;

            if (auto status = std::get_if<nx::vms::api::LdapStatus>(&errorOrData))
            {
                NX_ASSERT(success);

                d->online = status->state == nx::vms::api::LdapStatus::State::online;
                d->lastSync = status->timeSinceSyncS
                    ? nx::vms::time::fromNow(*status->timeSinceSyncS)
                    : QString();

                d->syncIsRunning = status->isRunning;

                d->updateUserAndGroupCount();
            }
            else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
            {
                d->online = false;
            }
        });

    if (d->statusHandle)
        connectedServerApi()->cancelRequest(d->statusHandle);

    d->statusHandle = connectedServerApi()->getLdapStatusAsync(
        statusCallback,
        thread());
}

void LdapSettingsWidget::loadDataToUi()
{
    loadSettings();
}

void LdapSettingsWidget::loadSettings(bool forceUpdate)
{
    if (!d->isAdmin())
        return;

    const auto loadSettingsCallback = nx::utils::guarded(this,
        [this, forceUpdate](
            bool success, int handle, rest::ErrorOrData<nx::vms::api::LdapSettings> errorOrData)
        {
            if (handle == d->currentHandle)
                d->currentHandle = 0;

            if (auto settings = std::get_if<nx::vms::api::LdapSettings>(&errorOrData))
            {
                NX_ASSERT(success);

                if (forceUpdate || !d->modified)
                {
                    d->initialSettings = *settings;
                    d->setState(d->initialSettings);
                    d->modified = false;
                }
                else
                {
                    d->initialSettings = *settings;
                    d->modified = d->getState() != d->initialSettings;
                }

                const auto state = d->getState();

                const bool serverChanged = d->initialSettings.uri != state.uri;
                if (serverChanged)
                {
                    d->userCount = -1;
                    d->groupCount = -1;
                    d->lastSync = "";
                    testOnline(
                        state.uri.toString(),
                        state.adminDn,
                        state.adminPassword.value_or(""));
                }
                else if (settings->isValid(/*checkPassword=*/ false))
                {
                    // Status request only shows results for settings already saved on the server.
                    if (!d->modified)
                        checkStatus();
                }
            }
            else
            {
                // This callback is triggered periodically, so avoid showing a messagebox with
                // the error. Most likely the server is disconnected.
                if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                    NX_ERROR(this, "Unable to load LDAP settings: %1", error);
                else
                    NX_ERROR(this, "Unable to load LDAP settings");

                d->online = false;
            }
        });

    if (d->currentHandle)
        connectedServerApi()->cancelRequest(d->currentHandle);

    d->currentHandle = connectedServerApi()->getLdapSettingsAsync(loadSettingsCallback, thread());
}

void LdapSettingsWidget::resetLdap()
{
    const QString mainText = tr("Disconnect LDAP server?");
    const QString extraText =
        tr("All LDAP users and groups will be deleted from the system.<br><br>"
        "LDAP settings will be also deleted.");

    QnMessageBox messageBox(
        QnMessageBoxIcon::Question,
        mainText,
        extraText,
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        this);
    messageBox.addButton(tr("Disconnect"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    if (messageBox.exec() != QDialogButtonBox::AcceptRole)
        return;

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Reset Settings"),
        tr("Enter your account password"),
        tr("Reset"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    const auto resetCallback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const rest::ErrorOrEmpty& result)
        {
            if (handle == d->currentHandle)
                d->currentHandle = 0;

            if (success)
            {
                d->initialSettings = {};
                d->setState(d->initialSettings);
                d->modified = false;
                return;
            }

            if (auto error = std::get_if<nx::network::rest::Result>(&result))
            {
                showError(error->errorString);
            }
            else
            {
                showError(tr("Connection failed"));
            }
        });

    if (d->currentHandle)
        connectedServerApi()->cancelRequest(d->currentHandle);

    d->currentHandle = connectedServerApi()->resetLdapAsync(
        sessionTokenHelper,
        resetCallback,
        thread());
}

void LdapSettingsWidget::applyChanges()
{
    if (!hasChanges())
        return;

    bool removeExisting = false;

    const bool firstImport = d->initialSettings.uri.isEmpty()
        && d->getLdapUserCount() <= 0
        && d->getLdapGroupCount() <= 0;

    if (d->initialSettings.uri.host() != d->getState().uri.host() && !firstImport)
    {
        const QString mainText = tr("Remove existing LDAP users and groups?");
        const QString extraText = tr("Looks like you have changed LDAP server. It is recommended"
            " to remove all existing LDAP users and groups before importing users and groups from"
            " a new LDAP server.");

        QnMessageBox messageBox(
            QnMessageBoxIcon::Question,
            mainText,
            extraText,
            QDialogButtonBox::Cancel | QDialogButtonBox::No,
            QDialogButtonBox::NoButton,
            this);
        messageBox.addButton(tr("Yes"), QDialogButtonBox::YesRole, Qn::ButtonAccent::Warning);

        switch (messageBox.exec())
        {
            case QDialogButtonBox::AcceptRole:
                removeExisting = true;
                break;
            case QDialogButtonBox::No:
                break;
            default:
                return;
        }
    }

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Apply Settings"),
        tr("Enter your account password"),
        tr("Apply"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    nx::vms::api::LdapSettingsChange settingsChange = {d->getState()};
    settingsChange.removeRecords = removeExisting;

    const auto settingsCallback = nx::utils::guarded(this,
        [this](
            bool success, int handle, rest::ErrorOrData<nx::vms::api::LdapSettings> errorOrData)
        {
            if (handle == d->currentHandle)
                d->currentHandle = 0;

            if (auto settings = std::get_if<nx::vms::api::LdapSettings>(&errorOrData))
            {
                NX_ASSERT(success);

                d->initialSettings = *settings;
                d->setState(d->initialSettings);
                d->modified = false;

                checkStatus();
            }
            else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
            {
                showError(error->errorString);
            }
            else
            {
                showError(tr("Connection failed"));
            }
        });

    if (d->currentHandle)
        connectedServerApi()->cancelRequest(d->currentHandle);

    d->currentHandle = connectedServerApi()->modifyLdapSettingsAsync(
        settingsChange,
        sessionTokenHelper,
        settingsCallback,
        thread());
}

void LdapSettingsWidget::requestSync()
{
    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Synchronize LDAP Users and Groups"),
        tr("Enter your account password"),
        tr("Synchronize"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    if (d->currentHandle)
        connectedServerApi()->cancelRequest(d->currentHandle);

    d->syncRequested = true;

    d->currentHandle = connectedServerApi()->syncLdapAsync(
        sessionTokenHelper,
        nx::utils::guarded(this,
            [this](bool success, int handle, const rest::ErrorOrEmpty& result)
            {
                if (handle == d->currentHandle)
                    d->currentHandle = 0;

                d->syncRequested = false;

                if (success)
                    checkStatus();
                else if (auto error = std::get_if<nx::network::rest::Result>(&result))
                    showError(error->errorString);
                else
                    showError(tr("Connection failed"));
            }),
        thread());
}

bool LdapSettingsWidget::hasChanges() const
{
    return d->modified;
}

void LdapSettingsWidget::testConnection(
    const QString& url,
    const QString& adminDn,
    const QString& password)
{
    d->testMessage = "";

    nx::vms::api::LdapSettings settings;
    settings.uri = nx::utils::Url::fromUserInput(url);
    settings.adminDn = adminDn;
    settings.adminPassword = password;

    if (d->currentHandle)
        connectedServerApi()->cancelRequest(d->currentHandle);

    d->currentHandle = connectedServerApi()->testLdapSettingsAsync(
        settings,
        nx::utils::guarded(this,
            [this](
                bool success, int handle, rest::ErrorOrData<std::vector<QString>> errorOrData)
            {
                if (handle == d->currentHandle)
                {
                    d->currentHandle = 0;
                }
                else
                {
                    d->testMessage = "";
                    return;
                }

                if (auto firstDnPerFilter = std::get_if<std::vector<QString>>(&errorOrData))
                {
                    d->testMessage = tr("Connection OK");
                    d->testState = TestState::ok;
                }
                else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                {
                    d->testMessage = error->errorString;
                    d->testState = TestState::error;
                }
                else
                {
                    d->testMessage = tr("Connection failed");
                    d->testState = TestState::error;
                }
            }),
        thread());

    if (d->currentHandle)
        d->testState = TestState::connecting;
}

void LdapSettingsWidget::testOnline(
    const QString& url,
    const QString& adminDn,
    const QString& password)
{
    d->checkingStatus = true;

    nx::vms::api::LdapSettings settings;
    settings.uri = nx::utils::Url::fromUserInput(url);
    settings.adminDn = adminDn;
    settings.adminPassword = password;

    if (d->currentHandle)
        connectedServerApi()->cancelRequest(d->currentHandle);

    d->currentHandle = connectedServerApi()->testLdapSettingsAsync(
        settings,
        nx::utils::guarded(this,
            [this](
                bool success, int handle, rest::ErrorOrData<std::vector<QString>> errorOrData)
            {
                if (handle == d->currentHandle)
                {
                    d->currentHandle = 0;
                    d->checkingStatus = false;
                }

                d->online = success && std::get_if<std::vector<QString>>(&errorOrData);
                d->updateUserAndGroupCount();
            }),
        thread());
}

void LdapSettingsWidget::showError(const QString& errorMessage)
{
     // Do not show errors from a cancelled request.
    if (d->currentHandle)
        return;

    QnMessageBox messageBox(
        QnMessageBoxIcon::Critical,
        tr("Failed to apply changes"),
        errorMessage,
        QDialogButtonBox::Ok,
        QDialogButtonBox::Ok,
        this);

    messageBox.setWindowTitle(tr("LDAP"));

    messageBox.exec();
}

void LdapSettingsWidget::cancelCurrentRequest()
{
    if (d->currentHandle)
    {
        connectedServerApi()->cancelRequest(d->currentHandle);
        d->currentHandle = 0;
    }
}

void LdapSettingsWidget::showEvent(QShowEvent* event)
{
    d->statusUpdateTimer.start();
}

void LdapSettingsWidget::hideEvent(QHideEvent* event)
{
    d->statusUpdateTimer.stop();
}

void LdapSettingsWidget::resetWarnings()
{
    d->hideEmptyLdapWarning = false;
}

} // namespace nx::vms::client::desktop

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
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_administration/globals/results_reporter.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/utils/ldap_status_watcher.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/time/formatter.h>
#include <ui/dialogs/common/message_box.h>
#include <utils/common/synctime.h>

#include "../models/ldap_filters_model.h"

namespace {

static const QUrl kLdapSettingsQmlComponentUrl("Nx/Dialogs/Ldap/Tabs/LdapSettings.qml");
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

    QmlProperty<bool> checkingOnlineStatus;
    QmlProperty<bool> online;
    QmlProperty<bool> syncIsRunning;
    QmlProperty<int> syncMode;
    QmlProperty<bool> syncRequested;
    QmlProperty<bool> hideEmptyLdapWarning;
    QmlProperty<bool> hideEditingWarning;

    QmlProperty<bool> modified;

    const QmlProperty<bool> hasConfig;

    nx::vms::api::LdapSettings initialSettings;

    rest::Handle currentHandle = 0;

    QTimer statusUpdateTimer;

    Private(LdapSettingsWidget* parent):
        q(parent),
        quickWidget(new QQuickWidget(appContext()->qmlEngine(), parent)),
        self(quickWidget, "self"),
        testState(quickWidget, "testState"),
        testMessage(quickWidget, "testMessage"),
        lastSync(quickWidget, "lastSync"),
        userCount(quickWidget, "userCount"),
        groupCount(quickWidget, "groupCount"),
        checkingOnlineStatus(quickWidget, "checkingOnlineStatus"),
        online(quickWidget, "online"),
        syncIsRunning(quickWidget, "syncIsRunning"),
        syncMode(quickWidget, "syncMode"),
        syncRequested(quickWidget, "syncRequested"),
        hideEmptyLdapWarning(quickWidget, "hideEmptyLdapWarning"),
        hideEditingWarning(quickWidget, "hideEditingWarning"),
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
                if (!modified
                    && q->globalSettings()->ldap().isValid(/*checkPassword=*/ false)
                    && currentHandle == 0)
                {
                    q->checkOnlineAndSyncStatus();
                }
            });

        connect(parent->globalSettings(), &common::SystemSettings::ldapSettingsChanged, q,
            [this]()
            {
                mergeUpdate(q->globalSettings()->ldap());
            });

        q->connect(q->systemContext()->ldapStatusWatcher(), &LdapStatusWatcher::refreshFinished,
            [this]() { handleLdapStatusUpdate(); });
    }

    void mergeUpdate(const nx::vms::api::LdapSettings& incoming)
    {
        const auto newState = LdapState::mergeStates(
            stateFromApi(initialSettings),
            LdapState::objectToState(quickWidget->rootObject()),
            stateFromApi(incoming));

        LdapState::stateToObject(newState, quickWidget->rootObject());
        initialSettings = incoming;
        modified = getState() != initialSettings;

        const bool serverChanged = initialSettings.uri.toString() != newState.uri;
        if (serverChanged)
        {
            userCount = -1;
            groupCount = -1;
            lastSync = "";

            q->testOnline(
                newState.uri,
                newState.adminDn,
                newState.password,
                newState.startTls,
                newState.ignoreCertErrors);
        }
    }

    static LdapSettings stateFromApi(const nx::vms::api::LdapSettings& settings)
    {
        LdapSettings state;

        state.uri = settings.uri.toString();
        state.adminDn = settings.adminDn;
        state.password = settings.adminPassword.value_or("");
        state.startTls = settings.startTls;
        state.ignoreCertErrors = settings.ignoreCertificateErrors;
        state.syncIntervalS = settings.continuousSyncIntervalS.count();
        state.searchTimeoutS = settings.searchTimeoutS.count();
        for (const auto& filter: settings.filters)
        {
            state.filters.push_back({
                .name = filter.name,
                .base = filter.base,
                .filter = filter.filter
            });
        }

        state.continuousSyncEnabled =
            settings.continuousSync != nx::vms::api::LdapSyncMode::disabled;
        state.continuousSync = (int) settings.continuousSync;

        state.loginAttribute = settings.loginAttribute;
        state.groupObjectClass = settings.groupObjectClass;
        state.memberAttribute = settings.memberAttribute;

        state.preferredSyncServer = settings.preferredMasterSyncServer;
        state.isHttpDigestEnabledOnImport = settings.isHttpDigestEnabledOnImport;
        state.isHttpDigestEnabledOnImportInitial = settings.isHttpDigestEnabledOnImport;

        return state;
    }

    void setState(const nx::vms::api::LdapSettings& settings)
    {
        LdapState::stateToObject(stateFromApi(settings), quickWidget->rootObject());
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
        settings.startTls = state.startTls;
        settings.ignoreCertificateErrors = state.ignoreCertErrors;
        settings.continuousSyncIntervalS = std::chrono::seconds(state.syncIntervalS);
        settings.searchTimeoutS = std::chrono::seconds(state.searchTimeoutS);

        settings.filters.clear();
        for (const auto& filter: state.filters)
        {
            settings.filters.push_back({
                .name = filter.name,
                .base = filter.base,
                .filter = filter.filter
            });
        }

        settings.continuousSync = (nx::vms::api::LdapSyncMode) state.continuousSync;

        settings.loginAttribute = state.loginAttribute;
        settings.groupObjectClass = state.groupObjectClass;
        settings.memberAttribute = state.memberAttribute;

        settings.preferredMasterSyncServer = state.preferredSyncServer;
        settings.isHttpDigestEnabledOnImport = state.isHttpDigestEnabledOnImport;

        return settings;
    }

    void updateUserAndGroupCount()
    {
        userCount = getLdapUserCount();
        groupCount = getLdapGroupCount();
    }

    void handleLdapStatusUpdate()
    {
        checkingOnlineStatus = false;

        const auto status = q->systemContext()->ldapStatusWatcher()->status();
        if (!status)
        {
            online = false;
            return;
        }

        online = (status->state == api::LdapStatus::State::online);
        lastSync = status->timeSinceSyncS
            ? time::fromNow(*status->timeSinceSyncS)
            : QString();
        syncIsRunning = status->isRunning;
        syncMode = (int) status->mode;

        updateUserAndGroupCount();
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

    setHelpTopic(this, HelpTopic::Ldap);
}

LdapSettingsWidget::~LdapSettingsWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void LdapSettingsWidget::discardChanges()
{
    d->statusUpdateTimer.stop();

    cancelCurrentRequest();
    d->setState(d->initialSettings);
}

void LdapSettingsWidget::checkOnlineAndSyncStatus()
{
    systemContext()->ldapStatusWatcher()->refresh();
}

void LdapSettingsWidget::loadDataToUi()
{
    d->initialSettings = globalSettings()->ldap();
    d->setState(d->initialSettings);
    d->modified = false;

    d->updateUserAndGroupCount();

    d->checkingOnlineStatus = true;
    checkOnlineAndSyncStatus();
}

bool LdapSettingsWidget::requestLdapReset()
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
        return false;

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Reset Settings"),
        tr("Enter your account password"),
        tr("Reset"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    nx::vms::api::LdapSettings settingsBackup = globalSettings()->ldap();

    const auto resetCallback = nx::utils::guarded(this,
        [this, settingsBackup = std::move(settingsBackup)](
            bool success, rest::Handle handle, const rest::ErrorOrEmpty& result)
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

            globalSettings()->setLdap(settingsBackup);

            if (auto error = std::get_if<nx::network::rest::Result>(&result))
            {
                if (error->error != nx::network::rest::Result::SessionExpired)
                    showError(error->errorString);
            }
            else
            {
                showError(tr("Connection failed"));
            }
        });

    cancelCurrentRequest();

    // Clear the settings immediately but rollback on error.
    globalSettings()->setLdap({});

    if (auto api = connectedServerApi())
        d->currentHandle = api->resetLdapAsync(sessionTokenHelper, resetCallback, thread());

    return true;
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

    nx::vms::api::LdapSettings settingsBackup = globalSettings()->ldap();

    nx::vms::api::LdapSettings settingsChange = {d->getState()};
    settingsChange.removeRecords = removeExisting;

    const auto settingsCallback = nx::utils::guarded(this,
        [this, settingsBackup = std::move(settingsBackup)](
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

                globalSettings()->setLdap(*settings);

                checkOnlineAndSyncStatus();
            }
            else
            {
                d->checkingOnlineStatus = false;
                globalSettings()->setLdap(settingsBackup);

                if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                {
                    if (error->error != nx::network::rest::Result::SessionExpired)
                        showError(error->errorString);
                }
                else
                {
                    showError(tr("Connection failed"));
                }
            }
        });

    cancelCurrentRequest();

    // Apply the settings immediately but rollback on error.
    globalSettings()->setLdap(settingsChange);
    d->checkingOnlineStatus = true;

    if (auto api = connectedServerApi())
    {
        d->currentHandle = api->modifyLdapSettingsAsync(
            settingsChange,
            sessionTokenHelper,
            settingsCallback,
            thread());
    }
}

void LdapSettingsWidget::requestSync()
{
    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Synchronize LDAP Users and Groups"),
        tr("Enter your account password"),
        tr("Synchronize"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    cancelCurrentRequest();
    d->syncRequested = true;

    auto callback = nx::utils::guarded(this,
        [this](bool success, int handle, const rest::ErrorOrEmpty& result)
        {
            if (handle == d->currentHandle)
                d->currentHandle = 0;

            d->syncRequested = false;

            if (success)
            {
                checkOnlineAndSyncStatus();
            }
            else if (auto error = std::get_if<nx::network::rest::Result>(&result))
            {
                if (error->error != nx::network::rest::Result::SessionExpired)
                    showError(error->errorString);
            }
            else
            {
                showError(tr("Connection failed"));
            }
        });

    if (auto api = connectedServerApi())
        d->currentHandle = api->syncLdapAsync(sessionTokenHelper, callback, thread());
}

bool LdapSettingsWidget::hasChanges() const
{
    return d->modified;
}

void LdapSettingsWidget::testConnection(
    const QString& url,
    const QString& adminDn,
    const QString& password,
    bool startTls,
    bool ignoreCertErrors)
{
    d->testMessage = "";

    nx::vms::api::LdapSettings settings;
    settings.uri = nx::utils::Url::fromUserInput(url);
    settings.adminDn = adminDn;
    settings.adminPassword = password.isEmpty() ? std::nullopt : std::make_optional(password);
    settings.startTls = startTls;
    settings.ignoreCertificateErrors = ignoreCertErrors;

    cancelCurrentRequest();

    auto callback = nx::utils::guarded(this,
        [this](
            bool /*success*/, int handle, rest::ErrorOrData<std::vector<QString>> errorOrData)
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
            else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData);
                error && error->error != network::rest::Result::ServiceUnavailable)
            {
                d->testMessage = error->errorString;
                d->testState = TestState::error;
            }
            else
            {
                d->testMessage = tr("Cannot connect to LDAP server");
                d->testState = TestState::error;
            }
        });

    if (auto api = connectedServerApi())
        d->currentHandle = api->testLdapSettingsAsync(settings, callback, thread());

    if (d->currentHandle > 0)
        d->testState = TestState::connecting;
}

void LdapSettingsWidget::testOnline(
    const QString& url,
    const QString& adminDn,
    const QString& password,
    bool startTls,
    bool ignoreCertErrors)
{
    d->checkingOnlineStatus = true;

    nx::vms::api::LdapSettings settings;
    settings.uri = nx::utils::Url::fromUserInput(url);
    settings.adminDn = adminDn;
    settings.adminPassword = password;
    settings.startTls = startTls;
    settings.ignoreCertificateErrors = ignoreCertErrors;

    cancelCurrentRequest();

    auto callback = nx::utils::guarded(this,
        [this](
            bool success, int handle, rest::ErrorOrData<std::vector<QString>> errorOrData)
        {
            if (handle == d->currentHandle)
                d->currentHandle = 0;

            d->checkingOnlineStatus = false;
            d->online = success && std::get_if<std::vector<QString>>(&errorOrData);
            d->updateUserAndGroupCount();
        });

    if (auto api = connectedServerApi())
        d->currentHandle = api->testLdapSettingsAsync(settings, callback, thread());
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
    if (auto api = connectedServerApi(); api && d->currentHandle > 0)
        api->cancelRequest(d->currentHandle);
    d->currentHandle = 0;
}

void LdapSettingsWidget::showEvent(QShowEvent*)
{
    d->statusUpdateTimer.start();
}

void LdapSettingsWidget::hideEvent(QHideEvent*)
{
    d->statusUpdateTimer.stop();
}

void LdapSettingsWidget::resetWarnings()
{
    d->hideEmptyLdapWarning = false;
    d->hideEditingWarning = false;
}

bool LdapSettingsWidget::isNetworkRequestRunning() const
{
    return d->currentHandle != 0;
}

} // namespace nx::vms::client::desktop

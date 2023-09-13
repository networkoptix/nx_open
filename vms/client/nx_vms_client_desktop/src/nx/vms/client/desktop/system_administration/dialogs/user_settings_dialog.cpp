// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_settings_dialog.h"

#include <algorithm>

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QPushButton>

#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <client_core/client_core_module.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/system_administration/globals/user_group_request_chain.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/resource/server_host_priority.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/utils/system_uri.h>
#include <recording/time_period.h>
#include <ui/dialogs/audit_log_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

#include "../globals/session_notifier.h"
#include "../models/members_model.h"

using namespace std::chrono;

namespace {

// TODO: #akolesnikov #move to cdb api section
static const QString cloudAuthInfoPropertyName("cloudUserAuthenticationInfo");

static constexpr auto kAuditTrailDays = 7;
static constexpr int kDefaultTempUserExpiresAfterLoginS = 60 * 60 * 8; //< 8 hours.

static const QString kAllowedLoginSymbols = "!#$%&'()*+,-./:;<=>?[]^_`{|}~";

bool isAcceptedLoginCharacter(QChar character)
{
    return character.isLetterOrNumber()
        || character == ' '
        || kAllowedLoginSymbols.contains(character);
}

enum LinkHostPriority
{
    cloud,
    dns,
    other,
    localHost
};

const auto customPriority =
    [](const nx::utils::Url& url) -> int
    {
        using namespace nx::vms::common;
        switch (serverHostPriority(url.host()))
        {
            case ServerHostPriority::cloud:
                return LinkHostPriority::cloud;
            case ServerHostPriority::dns:
                return LinkHostPriority::dns;
            case ServerHostPriority::localHost:
                return LinkHostPriority::localHost;
            default:
                return LinkHostPriority::other;
        }

        return LinkHostPriority::other;
    };

} // namespace

namespace nx::vms::client::desktop {

struct UserSettingsDialog::Private
{
    UserSettingsDialog* const q;
    QString syncId;
    QWidget* parentWidget = nullptr;
    QPointer<SessionNotifier> sessionNotifier;
    DialogType dialogType;
    QmlProperty<int> tabIndex;
    QmlProperty<bool> isSaving;
    QmlProperty<bool> ldapError;
    QmlProperty<bool> continuousSync;
    QmlProperty<AccessSubjectEditingContext*> editingContext;
    QmlProperty<UserSettingsDialog*> self; //< Used to call validate functions from QML.
    QmlProperty<int> displayOffsetMs;

    QmlProperty<QDateTime> linkValidFrom;
    QmlProperty<QDateTime> linkValidUntil;
    QmlProperty<int> expiresAfterLoginS;
    QmlProperty<bool> revokeAccessEnabled;
    QmlProperty<QDateTime> firstLoginTime;

    QnUserResourcePtr user;
    rest::Handle m_currentRequest = 0;

    Private(UserSettingsDialog* parent, DialogType dialogType):
        q(parent),
        syncId(parent->globalSettings()->ldap().syncId()),
        dialogType(dialogType),
        tabIndex(q->rootObjectHolder(), "tabIndex"),
        isSaving(q->rootObjectHolder(), "isSaving"),
        ldapError(q->rootObjectHolder(), "ldapError"),
        continuousSync(q->rootObjectHolder(), "continuousSync"),
        editingContext(q->rootObjectHolder(), "editingContext"),
        self(q->rootObjectHolder(), "self"),
        displayOffsetMs(q->rootObjectHolder(), "displayOffsetMs"),
        linkValidFrom(q->rootObjectHolder(), "linkValidFrom"),
        linkValidUntil(q->rootObjectHolder(), "linkValidUntil"),
        expiresAfterLoginS(q->rootObjectHolder(), "expiresAfterLoginS"),
        revokeAccessEnabled(q->rootObjectHolder(), "revokeAccessEnabled"),
        firstLoginTime(q->rootObjectHolder(), "firstLoginTime")
    {
        connect(parent->globalSettings(), &common::SystemSettings::ldapSettingsChanged, q,
            [this]()
            {
                syncId = q->globalSettings()->ldap().syncId();
                continuousSync = q->globalSettings()->ldap().continuousSync
                    == nx::vms::api::LdapSettings::Sync::usersAndGroups;
            });

        const auto updateDisplayOffset = [this](){ displayOffsetMs = getDisplayOffsetMs(); };

        connect(
            parent->systemContext()->serverTimeWatcher(),
            &core::ServerTimeWatcher::displayOffsetsChanged,
            q,
            updateDisplayOffset);

        updateDisplayOffset();

        continuousSync = q->globalSettings()->ldap().continuousSync
            == nx::vms::api::LdapSettings::Sync::usersAndGroups;
    }

    QDateTime serverDate(milliseconds msecsSinceEpoch) const
    {
        const auto timeWatcher = q->systemContext()->serverTimeWatcher();
        const auto server = timeWatcher->currentServer().dynamicCast<core::ServerResource>();

        if (!server) //< The server may absent on reconnection.
            return QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch.count());

        return timeWatcher->serverTime(server, msecsSinceEpoch.count());
    }

    int getDisplayOffsetMs() const
    {
        const auto serverNow = serverDate(
            duration_cast<milliseconds>(qnSyncTime->currentTimePoint()));

        const auto clientNow = QDateTime::currentDateTime();

        return duration_cast<milliseconds>(
            seconds(serverNow.offsetFromUtc() - clientNow.offsetFromUtc())).count();
    }

    QString linkFromToken(const std::string& token) const
    {
        const auto server = q->systemContext()->currentServer();
        const auto info = server->getModuleInformationWithAddresses();
        auto serverUrl = nx::vms::common::mainServerUrl(info.remoteAddresses, customPriority);
        const auto currentServerUrl = server->getApiUrl();
        const auto needChangeUrl = (customPriority(serverUrl) == LinkHostPriority::other
            && !nx::network::HostAddress(currentServerUrl.host()).isLoopback());

        if (needChangeUrl || serverUrl.isEmpty())
            serverUrl = currentServerUrl;

        if (customPriority(serverUrl) == LinkHostPriority::cloud)
            serverUrl.setHost(serverUrl.host().split('.').back());
        else if (serverUrl.port(-1) == -1)
            serverUrl.setPort(info.port);

        if (!ini().nativeLinkForTemporaryUsers)
            return nx::format("%1/#/?token=%2", serverUrl.displayAddress(), token);

        using namespace nx::vms::utils;
        SystemUri uri;
        uri.scope = SystemUri::Scope::direct;
        uri.protocol = SystemUri::Protocol::Native;
        uri.systemAddress = serverUrl.displayAddress();
        uri.credentials.authToken.setBearerToken(token);
        return uri.toString();
    }

    nx::vms::api::TemporaryToken generateTemporaryToken(
        const QDateTime& validUntil,
        int expiresAfterLoginS = -1) const
    {
        nx::vms::api::TemporaryToken token;

        token.startS = duration_cast<seconds>(qnSyncTime->currentTimePoint());
        token.endS = seconds(validUntil.toSecsSinceEpoch());
        if (expiresAfterLoginS != -1)
            token.expiresAfterLoginS = seconds(expiresAfterLoginS);

        return token;
    }

    void updateUiFromTemporaryToken(const nx::vms::api::TemporaryToken& temporaryToken)
    {
        linkValidFrom = serverDate(duration_cast<milliseconds>(temporaryToken.startS));
        linkValidUntil = serverDate(duration_cast<milliseconds>(temporaryToken.endS));

        revokeAccessEnabled = temporaryToken.expiresAfterLoginS.count() >= 0;

        expiresAfterLoginS = revokeAccessEnabled
            ? temporaryToken.expiresAfterLoginS.count()
            : -1;
    }

    nx::vms::api::UserModelV3 apiDataFromState(const UserSettingsDialogState& state) const
    {
        nx::vms::api::UserModelV3 userData;

        userData.type = (nx::vms::api::UserType) state.userType;

        const bool createCloudUser = dialogType == CreateUser
            && userData.type == nx::vms::api::UserType::cloud;

        if (createCloudUser)
        {
            userData.id = QnUuid::fromArbitraryData(state.email);
            userData.name = state.email;
        }
        else
        {
            userData.id = state.userId;
            userData.name = state.login;
        }

        if (!state.password.isEmpty()
            && !createCloudUser
            && userData.type != nx::vms::api::UserType::temporaryLocal)
        {
            userData.password = state.password;
        }
        userData.email = userData.type == nx::vms::api::UserType::cloud ? userData.name : state.email;
        userData.fullName = state.fullName;
        userData.permissions = state.globalPermissions;
        userData.isEnabled = state.userEnabled;
        userData.isHttpDigestEnabled = state.allowInsecure;
        for (const auto& group: state.parentGroups)
            userData.groupIds.emplace_back(group.id);

        const auto sharedResources = state.sharedResources.asKeyValueRange();
        userData.resourceAccessRights = {sharedResources.begin(), sharedResources.end()};

        return userData;
    }

    void showMessageBoxWithLink(const QString& title, const QString& text, const std::string& token)
    {
        const QString linkText = linkFromToken(token);

        QnMessageBox messageBox(
            QnMessageBoxIcon::Success,
            text,
            {},
            QDialogButtonBox::Ok,
            QDialogButtonBox::Ok,
            parentWidget);

        QPushButton* copyButton = new ClipboardButton(
            tr("Copy Access Link"),
            ClipboardButton::tr("Copied", "to Clipboard"));

        QObject::connect(copyButton, &QPushButton::pressed,
            [linkText]
            {
                QGuiApplication::clipboard()->setText(linkText);
            });

        messageBox.addCustomWidget(
            copyButton, QnMessageBox::Layout::Content, 0, Qt::AlignLeft);
        messageBox.setWindowTitle(title);
        messageBox.exec();
    }

    void showServerError(const QString& message, const nx::network::rest::Result& error)
    {
        QnMessageBox messageBox(
            QnMessageBoxIcon::Critical,
            message,
            error.errorString,
            QDialogButtonBox::Ok,
            QDialogButtonBox::Ok,
            parentWidget);
        messageBox.setWindowTitle(qApp->applicationDisplayName());
        messageBox.exec();
    }
};

UserSettingsDialog::UserSettingsDialog(
    DialogType dialogType,
    nx::vms::common::SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(parent, dialogType == EditUser
        ? "Nx/Dialogs/UserManagement/UserEditDialog.qml"
        : "Nx/Dialogs/UserManagement/UserCreateDialog.qml"),
    SystemContextAware(systemContext),
    d(new Private(this, dialogType))
{
    d->self = this;
    d->parentWidget = parent;

    if (parent)
    {
        d->sessionNotifier = new SessionNotifier(parent);
        connect(d->sessionNotifier, &SessionNotifier::closeRequested, this,
            [this]
            {
                reject();
            });

        connect(d->sessionNotifier, &SessionNotifier::forcedUpdateRequested, this,
            [this]
            {
                if (d->user || d->dialogType == CreateUser)
                    updateStateFrom(d->user);
            });
    }

    if (dialogType == EditUser)
    {
        // It is important to make the connections queued so we would not block inside QML code.
        connect(rootObjectHolder()->object(), SIGNAL(deleteRequested()),
            this, SLOT(onDeleteRequested()), Qt::QueuedConnection);

        connect(rootObjectHolder()->object(), SIGNAL(auditTrailRequested()),
            this, SLOT(onAuditTrailRequested()), Qt::QueuedConnection);
    }

    connect(rootObjectHolder()->object(), SIGNAL(addGroupRequested()),
        this, SLOT(onAddGroupRequested()), Qt::QueuedConnection);

    connect(systemContext->resourcePool(),
        &QnResourcePool::resourcesRemoved,
        this,
        [this](const QnResourceList& resources)
        {
            if (!d->user)
                return;

            for (const auto& resource: resources)
            {
                if (resource == d->user)
                {
                    reject();
                    setUser({}); //< reject() will not clear the user when the dialog is closed.
                    return;
                }
            }
        });

    connect(
        systemContext->accessRightsManager(),
        &nx::core::access::AbstractAccessRightsManager::ownAccessRightsChanged,
        this,
        [this](const QSet<QnUuid>& subjectIds)
        {
            if (d->user && subjectIds.contains(d->user->getId()))
                updateStateFrom(d->user);
        });

    // This is needed only at apply, because reject and accept clear current user.
    connect(this, &QmlDialogWrapper::applied, this,
        [this]()
        {
            if (d->editingContext)
                d->editingContext.value()->revert();
        });

    connect(this, &QmlDialogWrapper::rejected, this, [this]() { setUser({}); });
    connect(this, &QmlDialogWrapper::accepted, this, [this]() { setUser({}); });
}

UserSettingsDialog::~UserSettingsDialog()
{
}

QString UserSettingsDialog::validateCurrentPassword(const QString& password)
{
    if (password.isEmpty())
        return tr("To modify your password please enter the existing one.");

    if (!systemContext()->accessController()->user()->getHash().checkPassword(password))
        return tr("Invalid current password.");

    return {};
}

bool UserSettingsDialog::isConnectedToCloud() const
{
    return !systemContext()->globalSettings()->cloudSystemId().isEmpty();
}

QString UserSettingsDialog::validateEmail(const QString& email, bool forCloud)
{
    if (!forCloud)
    {
        const auto result = defaultEmailValidator()(email);
        return result.state != QValidator::Acceptable ? result.errorMessage : "";
    }

    TextValidateFunction validateFunction =
        [this](const QString& text) -> ValidationResult
        {
            auto result = defaultNonEmptyValidator(tr("Email cannot be empty"))(text);
            if (result.state != QValidator::Acceptable)
                return result;

            auto email = text.trimmed().toLower();
            for (const auto& user : resourcePool()->getResources<QnUserResource>())
            {
                if (!user->isCloud())
                    continue;

                if (user->getEmail().toLower() != email)
                    continue;

                return ValidationResult(
                    tr("%1 user with specified email already exists.",
                        "%1 is the short cloud name (like Cloud)")
                    .arg(nx::branding::shortCloudName()));
            }

            result = defaultEmailValidator()(text);
            return result;
        };

    const auto result = validateFunction(email);
    return result.state != QValidator::Acceptable ? result.errorMessage : "";
}

QString UserSettingsDialog::validateLogin(const QString& login)
{
    TextValidateFunction validateFunction =
        [this](const QString& text) -> ValidationResult
        {
            if (text.trimmed().isEmpty())
                return ValidationResult(tr("Login cannot be empty."));

            if (!std::all_of(text.cbegin(), text.cend(), &isAcceptedLoginCharacter))
            {
                return ValidationResult(
                    tr("Only letters, numbers and symbols %1 are allowed.")
                        .arg(kAllowedLoginSymbols));
            }

            for (const auto& user: resourcePool()->getResources<QnUserResource>())
            {
                if (user->getName().toLower() != text.toLower())
                    continue;

                // Allow our own login.
                if (d->user && d->user->getName() == text)
                    continue;

                if (d->dialogType == CreateUser && currentState().userId == user->getId())
                    continue;

                return ValidationResult(tr("User with specified login already exists."));
            }

            return ValidationResult::kValid;
        };

    const auto result = validateFunction(login);
    return result.state != QValidator::Acceptable ? result.errorMessage : "";
}

void UserSettingsDialog::onAddGroupRequested()
{
    d->sessionNotifier->actionManager()->trigger(ui::action::UserGroupsAction,
        ui::action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer(window())));
}

void UserSettingsDialog::onDeleteRequested()
{
    if (!NX_ASSERT(d->user))
        return;

    if (!ui::messages::Resources::deleteResources(
        appContext()->mainWindowContext()->workbenchContext()->mainWindowWidget(),
        {d->user},
        /*allowSilent*/ false))
    {
        return;
    }

    d->isSaving = true;

    auto callback = nx::utils::guarded(this,
        [this](bool success, const QnResourcePtr& /*resource*/)
        {
            d->isSaving = false;

            if (success)
                reject();
            else
                ui::messages::Resources::deleteResourcesFailed(d->parentWidget, {d->user});
        });

    qnResourcesChangesManager->deleteResource(d->user, callback);
}

void UserSettingsDialog::onAuditTrailRequested()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const auto timestampInPastMs = now.addDays(-kAuditTrailDays).toMSecsSinceEpoch();
    const auto durationMs = now.toMSecsSinceEpoch() - timestampInPastMs + 1;

    const QnTimePeriod period(timestampInPastMs, durationMs);

    d->sessionNotifier->actionManager()->trigger(
        ui::action::OpenAuditLogAction,
        ui::action::Parameters()
            .withArgument(Qn::TextRole, currentState().login)
            .withArgument(Qn::TimePeriodRole, period)
            .withArgument(Qn::FocusTabRole, (int) QnAuditLogDialog::sessionTabIndex)
            .withArgument(Qn::ParentWidgetRole, QPointer(window())));
}

void UserSettingsDialog::onTerminateLink()
{
    const QString mainText = tr("Are you sure you want to terminate access link?");
    const QString infoText = tr("This will instantly remove an access to the system for this user");

    QnSessionAwareMessageBox messageBox(d->parentWidget);

    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(mainText);
    messageBox.setInformativeText(infoText);
    messageBox.setStandardButtons(
        QDialogButtonBox::Discard
        | QDialogButtonBox::Cancel);

    messageBox.setDefaultButton(QDialogButtonBox::Discard, Qn::ButtonAccent::Warning);

    messageBox.button(QDialogButtonBox::Discard)->setText(tr("Terminate"));

    const auto ret = messageBox.exec();
    if (ret == QDialogButtonBox::Cancel)
        return;

    nx::vms::api::UserModelV3 userData = d->apiDataFromState(originalState());

    NX_ASSERT(userData.type == nx::vms::api::UserType::temporaryLocal);

    // Generate expired token.
    userData.temporaryToken = nx::vms::api::TemporaryToken{
        .startS = 2s,
        .endS = 1s,
        .expiresAfterLoginS = 0s
    };

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        d->parentWidget,
        tr("Terminate access link"),
        tr("Enter your account password"),
        tr("Terminate"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    if (d->m_currentRequest != 0)
        connectedServerApi()->cancelRequest(d->m_currentRequest);

    d->isSaving = true;

    d->m_currentRequest = connectedServerApi()->saveUserAsync(
        /*newUser=*/ false,
        userData,
        sessionTokenHelper,
        nx::utils::guarded(this,
            [this](
                bool success, int handle, rest::ErrorOrData<nx::vms::api::UserModelV3> errorOrData)
            {
                if (NX_ASSERT(handle == d->m_currentRequest))
                    d->m_currentRequest = 0;

                d->isSaving = false;

                if (success)
                    return;

                if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                {
                    d->showServerError(tr("Failed to apply changes"), *error);
                    return;
                }

                if (auto data = std::get_if<nx::vms::api::UserModelV3>(&errorOrData))
                {
                    NX_ASSERT(data->temporaryToken);
                    d->updateUiFromTemporaryToken(*data->temporaryToken);
                }
            }), thread());
}

void UserSettingsDialog::onResetLink(
    const QDateTime& validUntil,
    int revokeAccessAfterS,
    const QJSValue& callback)
{
    if (!NX_ASSERT(d->user)
        || !NX_ASSERT(d->user->userType() == nx::vms::api::UserType::temporaryLocal))
    {
        if (callback.isCallable())
            callback.call({false});
        return;
    }

    nx::vms::api::UserModelV3 userData = d->apiDataFromState(originalState());

    NX_ASSERT(userData.type == nx::vms::api::UserType::temporaryLocal);

    userData.temporaryToken = d->generateTemporaryToken(validUntil, revokeAccessAfterS);

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        d->parentWidget,
        tr("Create access link"),
        tr("Enter your account password"),
        tr("Create"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    if (d->m_currentRequest != 0)
        connectedServerApi()->cancelRequest(d->m_currentRequest);

    d->isSaving = true;

    d->m_currentRequest = connectedServerApi()->saveUserAsync(
        /*newUser=*/ false,
        userData,
        sessionTokenHelper,
        nx::utils::guarded(this,
            [this, callback](
                bool success, int handle, rest::ErrorOrData<nx::vms::api::UserModelV3> errorOrData)
            {
                if (NX_ASSERT(handle == d->m_currentRequest))
                    d->m_currentRequest = 0;

                d->isSaving = false;

                if (callback.isCallable())
                    callback.call({success});

                if (!success)
                {
                    if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                        d->showServerError(tr("Failed to apply changes"), *error);
                    return;
                }

                if (auto data = std::get_if<nx::vms::api::UserModelV3>(&errorOrData))
                {
                    NX_ASSERT(data->temporaryToken);

                    d->showMessageBoxWithLink(
                        tr("New Link - %1").arg(data->name),
                        tr("Access link has been successfully created!"),
                        data->temporaryToken->token);

                    d->updateUiFromTemporaryToken(*data->temporaryToken);
                }
            }), thread());
}

void UserSettingsDialog::cancelRequest()
{
    if (d->m_currentRequest != 0)
        connectedServerApi()->cancelRequest(d->m_currentRequest);
}

void UserSettingsDialog::onCopyLink()
{
    if (!NX_ASSERT(d->user))
        return;

    const QnUserHash hash = d->user->getHash();
    if (!NX_ASSERT(hash.type == QnUserHash::Type::temporary))
        return;

    if (!NX_ASSERT(hash.temporaryToken))
        return;

    if (!NX_ASSERT(!hash.temporaryToken->token.empty()))
        return;

    QGuiApplication::clipboard()->setText(d->linkFromToken(hash.temporaryToken->token));
}

QString UserSettingsDialog::warningForTemporaryUser(
    const QList<nx::vms::client::desktop::MembersModelGroup>& parentGroups,
    const nx::core::access::ResourceAccessMap& sharedResources,
    const nx::vms::api::GlobalPermissions permissions) const
{
    static constexpr nx::vms::api::GlobalPermissions kUserPermissions =
        nx::vms::api::GlobalPermission::viewLogs | nx::vms::api::GlobalPermission::generateEvents;

    const auto hasGroups =
        [this, &parentGroups](const QSet<QnUuid>& permissionGroups)
        {
            const auto hierarchy = systemContext()->accessSubjectHierarchy();

            for (const auto& group: parentGroups)
            {
                if (permissionGroups.contains(group.id)
                    || hierarchy->isRecursiveMember(group.id, permissionGroups))
                {
                    return true;
                }
            }

            return false;
        };
    
    const auto hasAccessRightAboveView =
        [&sharedResources]
        {
            return std::any_of(
                sharedResources.begin(),
                sharedResources.end(),
                [](const nx::vms::api::AccessRights& accessRights)
                {
                    return accessRights & ~nx::vms::api::kViewAccessRights;
                });
        };

    if (hasGroups({api::kAdministratorsGroupId, api::kPowerUsersGroupId}))
    {
        return tr("Granting broad permissions to the temporary user is not recommended."
            " Some actions may not work.");
    }
    else if (hasGroups({api::kAdvancedViewersGroupId, api::kViewersGroupId})
        || permissions & kUserPermissions || hasAccessRightAboveView())
    {
        return tr("Granting broad permissions to the temporary user is not recommended.");
    }

    return {};
}

QDateTime UserSettingsDialog::newValidUntilDate() const
{
    auto validUntil = d->serverDate(duration_cast<milliseconds>(qnSyncTime->currentTimePoint()))
        .addMonths(1);

    validUntil.setTime(QTime(23, 59, 59));

    return validUntil;
}

QString UserSettingsDialog::durationFormat(qint64 ms) const
{
    using namespace std::chrono;
    using namespace nx::vms::text;

    if (ms < 0) //< Prevent asserts in HumanReadable::timeSpan().
        return {};

    constexpr auto kOneMonth = months(1);
    static const QString separator =
        QString(" %1 ").arg(tr("and", /*comment*/ "Example: 1 month and 2 days"));

    int suppressSecondUnitLimit = text::HumanReadable::kAlwaysSuppressSecondUnit;

    auto duration = std::chrono::milliseconds(ms);

    if (const auto months = duration_cast<std::chrono::months>(duration); months >= kOneMonth)
        suppressSecondUnitLimit = HumanReadable::kNoSuppressSecondUnit;
    else if (const auto minutes = duration_cast<std::chrono::minutes>(duration); minutes < 1min)
        duration = 1min;

    return HumanReadable::timeSpan(
        duration_cast<std::chrono::milliseconds>(duration),
        HumanReadable::Months | HumanReadable::Days | HumanReadable::Hours | HumanReadable::Minutes,
        text::HumanReadable::SuffixFormat::Full,
        separator,
        suppressSecondUnitLimit);
}

UserSettingsDialogState UserSettingsDialog::createState(const QnUserResourcePtr& user)
{
    UserSettingsDialogState state;

    if (!user)
    {
        if (d->dialogType == CreateUser)
        {
            // We need non-null uuid to make editingContext happy.
            state.userId = QnUuid::createUuid();
            if (isConnectedToCloud())
                state.userType = UserSettingsGlobal::CloudUser;

            d->expiresAfterLoginS = kDefaultTempUserExpiresAfterLoginS;
            d->linkValidUntil = newValidUntilDate();
        }
        return state;
    }

    const bool isSelf = systemContext()->userWatcher()->user()->getId() == user->getId();
    Qn::Permissions permissions = accessController()->permissions(user);
    // Temporary user cannot edit itself.
    if (user->userType() == nx::vms::api::UserType::temporaryLocal && isSelf)
        permissions &= ~(Qn::FullUserPermissions | Qn::SavePermission);

    state.userType = (UserSettingsGlobal::UserType) user->userType();
    state.isSelf = isSelf;
    state.userId = user->getId();
    state.login = user->getName();
    state.loginEditable = permissions.testFlag(Qn::WriteNamePermission);
    state.fullName = user->fullName();
    state.fullNameEditable = permissions.testFlag(Qn::WriteFullNamePermission);
    state.email = user->getEmail();
    state.emailEditable = permissions.testFlag(Qn::WriteEmailPermission);
    state.passwordEditable = permissions.testFlag(Qn::WritePasswordPermission);
    state.userEnabled = user->isEnabled();
    state.userEnabledEditable = permissions.testFlag(Qn::WriteAccessRightsPermission);
    state.allowInsecure = user->digestAuthorizationEnabled();
    state.allowInsecureEditable = permissions.testFlag(Qn::WriteDigestPermission);

    state.auditAvailable = accessController()->hasPowerUserPermissions();
    state.deleteAvailable = permissions.testFlag(Qn::RemovePermission);

    state.parentGroupsEditable = permissions.testFlag(Qn::WriteAccessRightsPermission);

    // List of groups.
    for (const QnUuid& groupId: user->groupIds())
        state.parentGroups.insert(MembersModelGroup::fromId(systemContext(), groupId));

    state.sharedResources = systemContext()->accessRightsManager()->ownResourceAccessMap(
        user->getId());

    state.globalPermissions = user->getRawPermissions();

    state.permissionsEditable = permissions.testFlag(Qn::WriteAccessRightsPermission);

    if (user->userType() == nx::vms::api::UserType::temporaryLocal)
    {
        const QnUserHash hash = user->getHash();

        if (hash.type == QnUserHash::Type::temporary && hash.temporaryToken)
        {
            d->updateUiFromTemporaryToken(*hash.temporaryToken);

            const auto firstLoginTimeStr =
                user->getProperty(ResourcePropertyKey::kTemporaryUserFirstLoginTime);

            d->firstLoginTime = firstLoginTimeStr.isEmpty()
                ? QDateTime{}
                : d->serverDate(duration_cast<milliseconds>(
                    seconds(firstLoginTimeStr.toLongLong())));
        }
        else
        {
            d->updateUiFromTemporaryToken({});
        }
    }

    d->ldapError =
        user->isLdap() && !user->externalId().dn.isEmpty() && user->externalId().syncId != d->syncId;

    state.linkEditable = accessController()->hasPowerUserPermissions()
        && permissions.testFlag(Qn::SavePermission);

    return state;
}

void UserSettingsDialog::saveState(const UserSettingsDialogState& state)
{
    if (!NX_ASSERT(d->user || d->dialogType == CreateUser))
        return;

    if (d->dialogType == EditUser && !isModified())
    {
        saveStateComplete(state);
        return;
    }

    nx::vms::api::UserModelV3 userData = d->apiDataFromState(state);

    if (userData.type == nx::vms::api::UserType::temporaryLocal && d->dialogType == CreateUser)
    {
        userData.temporaryToken = d->generateTemporaryToken(
            d->linkValidUntil,
            d->revokeAccessEnabled
                ? d->expiresAfterLoginS
                : -1);
    }

    // When user changes his own password or digest support, current session credentials should be
    // updated correspondingly. Store actual password to update it in callback.
    std::optional<QString> actualPassword;
    if (d->user == systemContext()->userWatcher()->user())
    {
        // Changing current user password OR enabling digest authentication.
        if (userData.password)
        {
            actualPassword = userData.password;
        }
        // Disabling digest authentication.
        else if (d->user->digestAuthorizationEnabled() && !userData.isHttpDigestEnabled)
        {
            const auto credentials = systemContext()->connectionCredentials();
            if (NX_ASSERT(credentials.authToken.isPassword()))
                actualPassword = QString::fromStdString(credentials.authToken.value);
        }
    }

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        d->parentWidget,
        tr("Save user"),
        tr("Enter your account password"),
        tr("Save"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    if (d->m_currentRequest != 0)
        connectedServerApi()->cancelRequest(d->m_currentRequest);

    d->isSaving = true;

    d->m_currentRequest = connectedServerApi()->saveUserAsync(
        d->dialogType == CreateUser,
        userData,
        sessionTokenHelper,
        nx::utils::guarded(this,
            [this, state, actualPassword, sessionTokenHelper](
                bool success, int handle, rest::ErrorOrData<nx::vms::api::UserModelV3> errorOrData)
            {
                if (NX_ASSERT(handle == d->m_currentRequest))
                    d->m_currentRequest = 0;

                d->isSaving = false;

                if (!success)
                {
                    if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                        d->showServerError(tr("Failed to apply changes"), *error);
                    return;
                }

                if (actualPassword)
                {
                    if (auto currentSession = systemContext()->session())
                        currentSession->updatePassword(*actualPassword);
                }

                if (auto data = std::get_if<nx::vms::api::UserModelV3>(&errorOrData))
                {
                    if (data->type == nx::vms::api::UserType::temporaryLocal)
                    {
                        if (d->dialogType == CreateUser)
                        {
                            NX_ASSERT(data->temporaryToken);

                            d->showMessageBoxWithLink(
                                tr("New User"),
                                tr("User %1 has been successfully created!")
                                    .arg(common::html::colored(
                                        data->name,
                                        core::colorTheme()->color("light4"))),
                                data->temporaryToken->token);
                        }
                        else if (data->temporaryToken)
                        {
                            d->updateUiFromTemporaryToken(*data->temporaryToken);
                        }
                    }

                    if (d->user)
                    {
                        // Update user locally ahead of receiving update from the server
                        // to avoid UI blinking.
                        d->user->setName(data->name);
                        d->user->setEmail(state.email);
                        d->user->setFullName(state.fullName);
                        d->user->setRawPermissions(state.globalPermissions);
                        d->user->setEnabled(state.userEnabled);
                        d->user->setGroupIds(data->groupIds);
                    }

                    UserGroupRequestChain::updateLayoutSharing(
                        systemContext(), data->id, data->resourceAccessRights);

                    // Update access rights locally.
                    systemContext()->accessRightsManager()->setOwnResourceAccessMap(data->id,
                        {data->resourceAccessRights.begin(), data->resourceAccessRights.end()});

                    // Changing password or disabling digest auth leads to reconnect,
                    // make sure the new token is issued for reconnect to succeed.
                    if (state.isSelf)
                    {
                        if (!state.password.isEmpty())
                        {
                            refreshToken(state.password);
                        }
                        else if (originalState().allowInsecure && !state.allowInsecure)
                        {
                            const auto password = sessionTokenHelper->password();
                            if (NX_ASSERT(!password.isEmpty()))
                                refreshToken(password);
                        }
                    }
                }

                saveStateComplete(state);
            }), thread());
}

void UserSettingsDialog::refreshToken(const QString& password)
{
    NX_ASSERT(!password.isEmpty());

    nx::vms::api::LoginSessionRequest loginRequest;
    loginRequest.username = QString::fromStdString(
        systemContext()->connectionCredentials().username);
    loginRequest.password = password;

    auto callback = nx::utils::guarded(
        this,
        [this](
            bool /*success*/,
            int handle,
            rest::ErrorOrData<nx::vms::api::LoginSession> errorOrData)
        {
            if (NX_ASSERT(handle == d->m_currentRequest))
                d->m_currentRequest = 0;

            if (auto session = std::get_if<nx::vms::api::LoginSession>(&errorOrData))
            {
                NX_DEBUG(this, "Received token with length: %1", session->token.length());

                if (NX_ASSERT(!session->token.empty()))
                {
                    auto credentials = connection()->credentials();
                    credentials.authToken = nx::network::http::BearerAuthToken(session->token);

                    auto tokenExpirationTime =
                        qnSyncTime->currentTimePoint() + session->expiresInS;

                    clientMessageProcessor()->holdConnection(
                        QnClientMessageProcessor::HoldConnectionPolicy::reauth);

                    connection()->updateCredentials(
                        credentials,
                        tokenExpirationTime);

                    const auto localSystemId = connection()->moduleInformation().localSystemId;
                    nx::vms::client::core::CredentialsManager::storeCredentials(
                        localSystemId,
                        credentials);
                }
            }
            else
            {
                auto error = std::get_if<nx::network::rest::Result>(&errorOrData);
                NX_INFO(this, "Can't receive token: %1", QJson::serialized(*error));
            }
        });

    if (auto api = connectedServerApi(); NX_ASSERT(api, "No Server connection"))
        d->m_currentRequest = api->loginAsync(loginRequest, std::move(callback), thread());
}

bool UserSettingsDialog::setUser(const QnUserResourcePtr& user)
{
    if (d->dialogType == EditUser && d->user == user)
        return true; //< Do not reset state upon setting the same user.

    if (d->dialogType == EditUser && d->user && user && isModified())
    {
        const QString mainText = tr("Apply changes?");

        QnSessionAwareMessageBox messageBox(d->parentWidget);

        messageBox.setIcon(QnMessageBoxIcon::Question);
        messageBox.setText(mainText);
        messageBox.setStandardButtons(
            QDialogButtonBox::Discard
            | QDialogButtonBox::Apply
            | QDialogButtonBox::Cancel);
        messageBox.setDefaultButton(QDialogButtonBox::Apply);

        // Default text is "Don't save", but spec says it should be "Discard" here.
        messageBox.button(QDialogButtonBox::Discard)->setText(tr("Discard"));

        switch (messageBox.exec())
        {
            case QDialogButtonBox::Apply:
                QMetaObject::invokeMethod(window(), "apply", Qt::DirectConnection);
                // Calling apply is async, so we can not continue here.
                return false;
            case QDialogButtonBox::Discard:
                break;
            case QDialogButtonBox::Cancel:
                return false;
        }
    }

    if (d->user)
        d->user->disconnect(this);

    d->user = user;

    if (user)
    {
        const auto updateState = [this]() { updateStateFrom(d->user); };

        connect(user.get(), &QnResource::propertyChanged, this, updateState);
        connect(user.get(), &QnUserResource::digestChanged, this, updateState);
        connect(user.get(), &QnUserResource::userGroupsChanged, this, updateState);
        connect(user.get(), &QnUserResource::nameChanged, this, updateState);
        connect(user.get(), &QnUserResource::fullNameChanged, this, updateState);
        connect(user.get(), &QnUserResource::permissionsChanged, this, updateState);
        connect(user.get(), &QnUserResource::enabledChanged, this, updateState);
        connect(user.get(), &QnUserResource::attributesChanged, this, updateState);
        connect(user.get(), &QnUserResource::temporaryTokenChanged, this,
            [this]()
            {
                if (!d->user)
                    return;

                const auto hash = d->user->getHash();

                if (hash.temporaryToken)
                    d->updateUiFromTemporaryToken(*hash.temporaryToken);
            });
    }
    else
    {
        d->tabIndex = 0;
    }

    d->isSaving = false;
    createStateFrom(user);

    return true;
}

void UserSettingsDialog::selectTab(Tab tab)
{
    if (!NX_ASSERT(tab >= 0 && tab < TabCount))
        return;

    d->tabIndex = tab;
}

} // namespace nx::vms::client::desktop

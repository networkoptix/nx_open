// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_settings_dialog.h"

#include <algorithm>

#include <QtGui/QGuiApplication>
#include <QtWidgets/QPushButton>

#include <api/server_rest_connection.h>
#include <client/client_globals.h>
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
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
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
#include <nx/vms/common/system_settings.h>
#include <recording/time_period.h>
#include <ui/dialogs/audit_log_dialog.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include "../globals/session_notifier.h"
#include "../models/members_model.h"

namespace {

// TODO: #akolesnikov #move to cdb api section
static const QString cloudAuthInfoPropertyName("cloudUserAuthenticationInfo");

static constexpr auto kAuditTrailDays = 7;

static const QString kAllowedLoginSymbols = "!#$%&'()*+,-./:;<=>?[]^_`{|}~";

bool isAcceptedLoginCharacter(QChar character)
{
    return character.isLetterOrNumber()
        || character == ' '
        || kAllowedLoginSymbols.contains(character);
}

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
    QmlProperty<AccessSubjectEditingContext*> editingContext;
    QmlProperty<UserSettingsDialog*> self; //< Used to call validate functions from QML.
    QnUserResourcePtr user;
    rest::Handle m_currentRequest = -1;

    Private(UserSettingsDialog* parent, DialogType dialogType):
        q(parent),
        syncId(parent->globalSettings()->ldap().syncId()),
        dialogType(dialogType),
        tabIndex(q->rootObjectHolder(), "tabIndex"),
        isSaving(q->rootObjectHolder(), "isSaving"),
        ldapError(q->rootObjectHolder(), "ldapError"),
        editingContext(q->rootObjectHolder(), "editingContext"),
        self(q->rootObjectHolder(), "self")
    {
        connect(parent->globalSettings(), &common::SystemSettings::ldapSettingsChanged,
            [this]() { syncId = q->globalSettings()->ldap().syncId(); });
    }
};

UserSettingsDialog::UserSettingsDialog(
    DialogType dialogType,
    nx::vms::common::SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(parent, dialogType == EditUser
        ? "qrc:/qml/Nx/Dialogs/UserManagement/UserEditDialog.qml"
        : "qrc:/qml/Nx/Dialogs/UserManagement/UserCreateDialog.qml"),
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

    connect(rootObjectHolder()->object(), SIGNAL(groupClicked(QVariant)),
        this, SLOT(onGroupClicked(QVariant)), Qt::QueuedConnection);

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
            auto result = defaultNonEmptyValidator(tr("Email cannot be empty."))(text);
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

                return ValidationResult(tr("User with specified login already exists."));
            }

            return ValidationResult::kValid;
        };

    const auto result = validateFunction(login);
    return result.state != QValidator::Acceptable ? result.errorMessage : "";
}

void UserSettingsDialog::onGroupClicked(const QVariant& idVariant)
{
    d->sessionNotifier->actionManager()->trigger(
        ui::action::UserGroupsAction, ui::action::Parameters()
            .withArgument(Qn::UuidRole, idVariant.value<QnUuid>())
            .withArgument(Qn::ParentWidgetRole, QPointer(window())));
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

    if (systemContext()->restApiHelper()->restApiEnabled())
    {
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
    else
    {
        auto callback = nx::utils::guarded(this,
            [this](bool success, const QString& /*errorString*/)
            {
                d->isSaving = false;

                if (success)
                    reject();
            });

        qnResourcesChangesManager->deleteResources({d->user}, callback);
    }
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
        }
        return state;
    }

    const auto currentUser = systemContext()->userWatcher()->user();
    const Qn::Permissions permissions = accessController()->permissions(user);

    state.userType = (UserSettingsGlobal::UserType) user->userType();
    state.isSelf = currentUser->getId() == user->getId();
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

    d->ldapError =
        user->isLdap() && !user->externalId().dn.isEmpty() && user->externalId().syncId != d->syncId;

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

    nx::vms::api::UserModelV3 userData;

    userData.type = (nx::vms::api::UserType) state.userType;

    const bool createCloudUser = d->dialogType == CreateUser
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

    if (!state.password.isEmpty() && !createCloudUser)
        userData.password = state.password;
    userData.email = userData.type == nx::vms::api::UserType::cloud ? userData.name : state.email;
    userData.fullName = state.fullName;
    userData.permissions = state.globalPermissions;
    userData.isEnabled = state.userEnabled;
    userData.isHttpDigestEnabled = state.allowInsecure;
    for (const auto& group: state.parentGroups)
        userData.groupIds.emplace_back(group.id);

    const auto sharedResources = state.sharedResources.asKeyValueRange();
    userData.resourceAccessRights = {sharedResources.begin(), sharedResources.end()};

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        d->parentWidget,
        tr("Save user"),
        tr("Enter your account password"),
        tr("Save"),
        FreshSessionTokenHelper::ActionType::updateSettings);

    if (d->m_currentRequest != -1)
        connectedServerApi()->cancelRequest(d->m_currentRequest);

    d->isSaving = true;

    d->m_currentRequest = connectedServerApi()->saveUserAsync(
        d->dialogType == CreateUser,
        userData,
        sessionTokenHelper,
        nx::utils::guarded(this,
            [this, state](
                bool success, int handle, rest::ErrorOrData<nx::vms::api::UserModelV3> errorOrData)
            {
                if (NX_ASSERT(handle == d->m_currentRequest))
                    d->m_currentRequest = -1;

                d->isSaving = false;

                if (!success)
                {
                    if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                    {
                        QnMessageBox messageBox(
                            QnMessageBoxIcon::Critical,
                            tr("Failed to apply changes"),
                            error->errorString,
                            QDialogButtonBox::Ok,
                            QDialogButtonBox::Ok,
                            d->parentWidget);
                        messageBox.setWindowTitle(qApp->applicationDisplayName());
                        messageBox.exec();
                    }
                    return;
                }

                if (auto data = std::get_if<nx::vms::api::UserModelV3>(&errorOrData))
                {
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
                }

                saveStateComplete(state);
            }), thread());
}

void UserSettingsDialog::setUser(const QnUserResourcePtr& user)
{
    if (d->dialogType == EditUser && d->user == user)
        return; //< Do not reset state upon setting the same user.

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
                return;
            case QDialogButtonBox::Discard:
                break;
            case QDialogButtonBox::Cancel:
                return;
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
    }
    else
    {
        d->tabIndex = 0;
    }

    d->isSaving = false;
    createStateFrom(user);
}

void UserSettingsDialog::selectTab(Tab tab)
{
    if (!NX_ASSERT(tab >= 0 && tab < TabCount))
        return;

    d->tabIndex = tab;
}

} // namespace nx::vms::client::desktop

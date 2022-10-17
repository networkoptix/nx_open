// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_settings_dialog.h"

#include <algorithm>

#include <QtGui/QGuiApplication>

#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/branding.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/system_settings.h>
#include <recording/time_period.h>
#include <ui/dialogs/audit_log_dialog.h>
#include <ui/models/resource_properties/user_settings_model.h>
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
    UserSettingsDialog* q;
    QPointer<SessionNotifier> sessionNotifier;
    DialogType dialogType;
    QmlProperty<int> tabIndex;
    QmlProperty<UserSettingsDialog*> self; //< Used to call validate functions from QML.
    QnUserResourcePtr user;

    Private(UserSettingsDialog* parent, DialogType dialogType):
        q(parent),
        dialogType(dialogType),
        tabIndex(q->rootObjectHolder(), "tabIndex"),
        self(q->rootObjectHolder(), "self")
    {
    }
};

UserSettingsDialog::UserSettingsDialog(
    DialogType dialogType,
    nx::vms::common::SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(parent, dialogType == editUser
        ? "qrc:/qml/Nx/Dialogs/UserManagement/UserEditDialog.qml"
        : "qrc:/qml/Nx/Dialogs/UserManagement/UserCreateDialog.qml"),
    SystemContextAware(systemContext),
    d(new Private(this, dialogType))
{
    d->self = this;

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
                updateStateFrom(d->user);
            });
    }

    if (dialogType == editUser)
    {
        // It is important to make the connections queued so we would not block inside QML code.
        connect(rootObjectHolder()->object(), SIGNAL(deleteRequested()),
            this, SLOT(onDeleteRequested()), Qt::QueuedConnection);
        connect(rootObjectHolder()->object(), SIGNAL(auditTrailRequested()),
            this, SLOT(onAuditTrailRequested()), Qt::QueuedConnection);
        connect(rootObjectHolder()->object(), SIGNAL(groupClicked(QVariant)),
            this, SLOT(onGroupClicked(QVariant)), Qt::QueuedConnection);
    }
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
                if (!d->user.isNull() && d->user->getName() == text)
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
        ui::action::UserRolesAction, ui::action::Parameters()
            .withArgument(Qn::UuidRole, idVariant.value<QnUuid>()));
}

void UserSettingsDialog::onDeleteRequested()
{
    if (!ui::messages::Resources::deleteResources(
        appContext()->mainWindowContext()->workbenchContext()->mainWindowWidget(), {d->user}))
    {
        return;
    }

    auto callback = nx::utils::guarded(this,
        [this](bool success)
        {
            if (success)
                reject();
        });

    qnResourcesChangesManager->deleteResources({d->user}, callback);
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
            .withArgument(Qn::FocusTabRole, (int) QnAuditLogDialog::sessionTabIndex));
}

UserSettingsDialogState UserSettingsDialog::createState(const QnUserResourcePtr& user)
{
    UserSettingsDialogState state;

    if (!user)
    {
        NX_ASSERT(d->dialogType == createUser);
        // We need non-null uuid to make editingContext happy.
        state.userId = QnUuid::createUuid();
        return state;
    }

    const Qn::Permissions permissions = accessController()->permissions(user);

    state.userType = UserSettingsGlobal::getUserType(user);
    state.isSelf = systemContext()->accessController()->user()->getId() == user->getId();
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

    state.auditAvailable = accessController()->hasAdminPermissions();
    state.deleteAvailable = permissions.testFlag(Qn::RemovePermission);

    state.parentGroupsEditable = permissions.testFlag(Qn::WriteAccessRightsPermission);

    // List of groups.
    for (const QnUuid& groupId: user->userRoleIds())
        state.parentGroups.insert(MembersModelGroup::fromId(systemContext(), groupId));

    state.sharedResources = systemContext()->accessRightsManager()->ownResourceAccessMap(
        user->getId());

    state.globalPermissions = user->getRawPermissions();

    return state;
}

void UserSettingsDialog::saveState(const UserSettingsDialogState& state)
{
    ResourcesChangesManager::UserChangesFunction applyChangesFunction = nx::utils::guarded(this,
        [this, state](const QnUserResourcePtr& /*user*/)
        {
            d->user->setName(state.login);
            d->user->setEmail(state.email);
            d->user->setEnabled(state.userEnabled);
            d->user->setFullName(state.fullName);

            std::vector<QnUuid> groupIds;
            groupIds.reserve(state.parentGroups.size());
            for (const auto& group: state.parentGroups)
                groupIds.push_back(group.id);

            d->user->setUserRoleIds(groupIds);
            d->user->setRawPermissions(state.globalPermissions);
        });

    auto saveAccessRightsCallback = nx::utils::guarded(this,
        [this, state](bool ok)
        {
            if (!ok)
                return;

            saveStateComplete(state);
        });

    ResourcesChangesManager::UserCallbackFunction callbackFunction = nx::utils::guarded(this,
        [state, saveAccessRightsCallback](bool success, const QnUserResourcePtr& user)
        {
            if (!success)
                return;

            qnResourcesChangesManager->saveAccessRights(
                user, state.sharedResources, saveAccessRightsCallback);
        });

    if (d->dialogType == createUser)
    {
        d->user.reset(new QnUserResource(state.userType == UserSettingsGlobal::LocalUser
            ? nx::vms::api::UserType::local
            : nx::vms::api::UserType::cloud,
            /*externalId*/ {}));
        d->user->setRawPermissions(state.globalPermissions);
        d->user->setName(state.login);
        d->user->setIdUnsafe(state.userId);
    }

    QnUserResource::DigestSupport digestSupport =
        originalState().allowInsecure == state.allowInsecure && d->dialogType != createUser
            ? QnUserResource::DigestSupport::keep
            : state.allowInsecure
                ? QnUserResource::DigestSupport::enable
                : QnUserResource::DigestSupport::disable;

    if (!state.password.isEmpty())
        d->user->setPasswordAndGenerateHash(state.password, digestSupport);

    qnResourcesChangesManager->saveUser(
        d->user,
        digestSupport,
        applyChangesFunction,
        callbackFunction);
}

void UserSettingsDialog::setUser(const QnUserResourcePtr& user)
{
    d->tabIndex = 0;

    if (d->user)
        disconnect(d->user.get());

    d->user = user;

    if (d->user)
    {
        const auto updateState = [this]{ updateStateFrom(d->user); };

        connect(d->user.get(), &QnResource::propertyChanged, this, updateState);
        connect(d->user.get(), &QnUserResource::digestChanged, this, updateState);
        connect(d->user.get(), &QnUserResource::userRolesChanged, this, updateState);
    }

    createStateFrom(d->user);
}

} // namespace nx::vms::client::desktop

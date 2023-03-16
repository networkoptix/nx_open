// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group_settings_dialog.h"

#include <client/client_globals.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/algorithm/diff_sorted_lists.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>

#include "../globals/session_notifier.h"
#include "../globals/user_group_request_chain.h"


namespace nx::vms::client::desktop {

namespace {

struct DifferencesResult
{
    QSet<QnUuid> removed;
    QSet<QnUuid> added;
};

/** Returns differences between two sets. */
template <typename SortedContainer>
DifferencesResult differences(
    const SortedContainer& original,
    const SortedContainer& current)
{
    DifferencesResult result;

    nx::utils::algorithm::full_difference(
        original.begin(),
        original.end(),
        current.begin(),
        current.end(),
        [&result](auto value) { result.removed.insert(value); },
        [&result](auto value) { result.added.insert(value); },
        [](auto&&...) {});

    return result;
}

} // namespace

struct GroupSettingsDialog::Private
{
    GroupSettingsDialog* q;
    QWidget* parentWidget = nullptr;
    QPointer<SessionNotifier> sessionNotifier;
    DialogType dialogType;
    QmlProperty<int> tabIndex;
    QmlProperty<GroupSettingsDialog*> self; //< Used to call validate functions from QML.

    QnUuid groupId;
    GroupSettingsDialogState originalState;

    Private(GroupSettingsDialog* parent, DialogType dialogType):
        q(parent),
        dialogType(dialogType),
        tabIndex(q->rootObjectHolder(), "tabIndex"),
        self(q->rootObjectHolder(), "self")
    {
    }

    bool hasCycles(const GroupSettingsDialogState& state)
    {
        QSet<QnUuid> directParents;
        for (const auto& group: state.parentGroups)
            directParents.insert(group.id);

        const auto allParents =
            q->systemContext()->accessSubjectHierarchy()->recursiveParents(directParents)
            + directParents;

        if (allParents.contains(state.groupId))
            return true;

        return std::any_of(state.groups.begin(), state.groups.end(),
            [&allParents](const auto& id){ return allParents.contains(id); });
    }
};

GroupSettingsDialog::GroupSettingsDialog(
    DialogType dialogType,
    nx::vms::common::SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(
        parent,
        dialogType == editGroup
            ? "qrc:/qml/Nx/Dialogs/UserManagement/GroupEditDialog.qml"
            : "qrc:/qml/Nx/Dialogs/UserManagement/GroupCreateDialog.qml"),
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
                updateStateFrom(d->groupId);
            });
    }

    connect(rootObjectHolder()->object(), SIGNAL(groupClicked(QVariant)),
        this, SLOT(onGroupClicked(QVariant)));

    if (dialogType == editGroup)
    {
        connect(rootObjectHolder()->object(), SIGNAL(deleteRequested()),
            this, SLOT(onDeleteRequested()));
    }

    connect(systemContext->userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
        [this](const nx::vms::api::UserRoleData& userGroup)
        {
            if (userGroup.id == d->groupId)
                updateStateFrom(userGroup.id);
        });

    connect(this, &QmlDialogWrapper::rejected, [this] { setGroup({}); });
}

GroupSettingsDialog::~GroupSettingsDialog()
{
}

QString GroupSettingsDialog::validateName(const QString& text)
{
    auto name = text.trimmed().toLower();

    auto rolesManager = systemContext()->userRolesManager();

    if (name.isEmpty())
        return tr("Role name cannot be empty.");

    const auto currentName = rolesManager->userRole(d->groupId).name.trimmed().toLower();

    for (const auto& group: rolesManager->userRoles())
    {
        // Allow our own name.
        if (!d->groupId.isNull() && currentName == name)
            continue;

        if (group.name.trimmed().toLower() == name)
            return tr("Role with same name already exists.");
    }

    auto predefined = QnPredefinedUserRoles::enumValues();
    predefined << Qn::UserRole::customPermissions << Qn::UserRole::customUserRole;
    for (auto role: predefined)
    {
        if (QnPredefinedUserRoles::name(role).trimmed().toLower() == name)
            return tr("Role with same name already exists.");
    }

    return {};
}

void GroupSettingsDialog::onGroupClicked(const QVariant& idVariant)
{
    d->sessionNotifier->actionManager()->trigger(
        ui::action::UserRolesAction,
        ui::action::Parameters()
            .withArgument(Qn::UuidRole, idVariant.value<QnUuid>())
            .withArgument(Qn::ParentWidgetRole, QPointer(window())));
}

void GroupSettingsDialog::onDeleteRequested()
{
    const auto handleRemove = nx::utils::guarded(this,
        [this](bool success)
        {
            if (success)
                reject();
        });

    removeGroups(systemContext(), {d->groupId}, std::move(handleRemove));
}

void GroupSettingsDialog::setGroup(const QnUuid& groupId)
{
    d->groupId = groupId;
    d->tabIndex = 0;
    createStateFrom(groupId);
}

GroupSettingsDialogState GroupSettingsDialog::createState(const QnUuid& groupId)
{
    GroupSettingsDialogState state;

    auto rolesManager = systemContext()->userRolesManager();

    if (d->dialogType == createGroup)
    {
        QStringList usedNames;
        for (const auto& userRole: rolesManager->userRoles())
            usedNames << userRole.name;

        state.name = nx::utils::generateUniqueString(
            usedNames, tr("New Group"), tr("New Group %1"));
        state.groupId = QnUuid::createUuid();
    }
    else
    {
        const auto groupData = rolesManager->userRole(groupId);

        state.name = groupData.name;
        state.groupId = groupId;
        state.description = groupData.description;

        state.globalPermissions =
            groupData.permissions & nx::vms::api::kNonDeprecatedGlobalPermissions;

        state.sharedResources = systemContext()->accessRightsManager()->ownResourceAccessMap(
            groupId);
        state.isLdap = (groupData.type == nx::vms::api::UserType::ldap);
        state.isPredefined = groupData.isPredefined;

        for (const auto& parentGroupId: groupData.parentGroupIds)
            state.parentGroups.insert(MembersModelGroup::fromId(systemContext(), parentGroupId));

        QList<QnUuid> users;
        for (const auto& user: systemContext()->resourcePool()->getResources<QnUserResource>())
        {
            const auto groupIds = user->userRoleIds();

            if (std::find(groupIds.begin(), groupIds.end(), groupId) != groupIds.end())
                users.append(user->getId());
        }
        std::sort(users.begin(), users.end());
        state.users = users;

        for (const auto& group: systemContext()->userRolesManager()->userRoles())
        {
            if (std::find(group.parentGroupIds.begin(), group.parentGroupIds.end(), groupId)
                != group.parentGroupIds.end())
            {
                state.groups.insert(group.id);
            }
        }
    }

    return state;
}

void GroupSettingsDialog::saveState(const GroupSettingsDialogState& state)
{
    if (d->hasCycles(state))
    {
        // TODO: show a banner about cycles.
        return;
    }

    UserGroupRequest::AddOrUpdateGroup updateData;

    updateData.newGroup = d->dialogType == createGroup;

    updateData.groupData.id = state.groupId;
    updateData.groupData.name = state.name;
    updateData.groupData.description = state.description;
    updateData.groupData.permissions = state.globalPermissions;
    updateData.groupData.parentGroupIds.clear();
    for (const auto& group: state.parentGroups)
        updateData.groupData.parentGroupIds.push_back(group.id);

    for (const auto& group: originalState().parentGroups)
        updateData.originalParents.push_back(group.id);

    updateData.groupData.type = state.isLdap
        ? nx::vms::api::UserType::ldap
        : nx::vms::api::UserType::local;

    const auto usersDiff = differences(originalState().users.list(), state.users.list());
    const auto groupsDiff = differences(originalState().groups, state.groups);

    const auto resourcePool = QPointer<QnResourcePool>(systemContext()->resourcePool());

    auto chain = new UserGroupRequestChain(systemContext(), /*parent*/ this);

    // Remove group from groups.

    for (const auto& id: groupsDiff.removed)
    {
        if (const auto userGroup = systemContext()->userRolesManager()->userRole(id);
            !userGroup.id.isNull())
        {
            UserGroupRequest::ModifyGroupParents mod;
            mod.id = id;
            mod.prevParents = userGroup.parentGroupIds;
            for (const auto& groupId: mod.prevParents)
            {
                if (groupId != updateData.groupData.id)
                    mod.newParents.emplace_back(groupId);
            }
            chain->append(mod);
        }
    }

    // Remove group from users.

    for (const auto& id: usersDiff.removed)
    {
        if (const auto user = resourcePool->getResourceById<QnUserResource>(id))
        {
            UserGroupRequest::ModifyUserParents mod;
            mod.id = id;
            mod.prevParents = user->userRoleIds();
            for (const auto& groupId: mod.prevParents)
            {
                if (groupId != updateData.groupData.id)
                    mod.newParents.emplace_back(groupId);
            }
            chain->append(mod);
        }
    }

    if (!state.isPredefined)
        chain->append(updateData);

    // Add to groups.

    for (const auto& id: groupsDiff.added)
    {
        if (const auto userGroup = systemContext()->userRolesManager()->userRole(id);
            !userGroup.id.isNull())
        {
            UserGroupRequest::ModifyGroupParents mod;
            mod.id = id;
            mod.prevParents = userGroup.parentGroupIds;
            mod.newParents = mod.prevParents;
            mod.newParents.emplace_back(updateData.groupData.id);
            chain->append(mod);
        }
    }

    // Add to users.

    for (const auto& userId: usersDiff.added)
    {
        if (const auto user = resourcePool->getResourceById<QnUserResource>(userId))
        {
            UserGroupRequest::ModifyUserParents mod;
            mod.id = userId;
            mod.prevParents = user->userRoleIds();
            mod.newParents = mod.prevParents;
            mod.newParents.emplace_back(updateData.groupData.id);
            chain->append(mod);
        }
    }

    chain->setTokenHelper(FreshSessionTokenHelper::makeHelper(
        d->parentWidget,
        tr("Save changes"),
        tr("Enter your account password"),
        tr("Save"),
        FreshSessionTokenHelper::ActionType::updateSettings));

    chain->start(nx::utils::guarded(this,
        [chain, state, this](
            bool success,
            const QString& errorString)
        {
            if (success)
            {
                saveStateComplete(state);
            }
            else
            {
                QnMessageBox messageBox(
                    QnMessageBoxIcon::Critical,
                    tr("Failed to apply changes"),
                    errorString,
                    QDialogButtonBox::Ok,
                    QDialogButtonBox::Ok,
                    d->parentWidget);
                messageBox.setWindowTitle(qApp->applicationDisplayName());
                messageBox.exec();
            }

            chain->deleteLater();
        }));
}

void GroupSettingsDialog::removeGroups(
    nx::vms::client::desktop::SystemContext* systemContext,
    const QSet<QnUuid>& idsToRemove,
    std::function<void(bool)> callback)
{
    auto chain = new UserGroupRequestChain(systemContext);

    // Remove groups from groups.

    for (const auto& group: systemContext->userRolesManager()->userRoles())
    {
        UserGroupRequest::ModifyGroupParents mod;
        for (const auto& groupId: group.parentGroupIds)
        {
            if (!idsToRemove.contains(groupId))
                mod.newParents.emplace_back(groupId);
        }

        if (group.parentGroupIds.size() == mod.newParents.size())
            continue;

        mod.id = group.id;
        mod.prevParents = group.parentGroupIds;
        chain->append(mod);
    }

    // Remove groups from users.

    const auto resourcePool = systemContext->resourcePool();

    for (const auto& user: resourcePool->getResources<QnUserResource>())
    {
        UserGroupRequest::ModifyUserParents mod;
        mod.prevParents = user->userRoleIds();
        for (const auto& groupId: mod.prevParents)
        {
            if (!idsToRemove.contains(groupId))
                mod.newParents.emplace_back(groupId);
        }
        if (mod.prevParents.size() == mod.newParents.size())
            continue;

        mod.id = user->getId();
        chain->append(mod);
    }

    // Remove groups.

    for (const auto& id: idsToRemove)
        chain->append(UserGroupRequest::RemoveGroup{.id = id});

    chain->setTokenHelper( FreshSessionTokenHelper::makeHelper(
        appContext()->mainWindowContext()->workbenchContext()->mainWindowWidget(),
        tr("Delete groups"),
        tr("Enter your account password"),
        tr("Delete"),
        FreshSessionTokenHelper::ActionType::updateSettings));

    chain->start(nx::utils::guarded(chain,
        [chain, callback = std::move(callback)](
            bool success,
            const QString& /*errorString*/)
        {
            if (callback)
                callback(success);
            chain->deleteLater();
        }));
}

} // namespace nx::vms::client::desktop

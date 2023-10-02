// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group_settings_dialog.h"

#include <QtWidgets/QPushButton>

#include <client/client_globals.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/algorithm/diff_sorted_lists.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/system_administration/watchers/non_editable_users_and_groups.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/messages/user_groups_messages.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <ui/dialogs/common/session_aware_dialog.h>
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
    QmlProperty<bool> isSaving;
    QmlProperty<AccessSubjectEditingContext*> editingContext;
    QmlProperty<GroupSettingsDialog*> self; //< Used to call validate functions from QML.
    QmlProperty<bool> continuousSync;
    QmlProperty<bool> deleteAvailable;

    QnUuid groupId;
    GroupSettingsDialogState originalState;

    Private(GroupSettingsDialog* parent, DialogType dialogType):
        q(parent),
        dialogType(dialogType),
        tabIndex(q->rootObjectHolder(), "tabIndex"),
        isSaving(q->rootObjectHolder(), "isSaving"),
        editingContext(q->rootObjectHolder(), "editingContext"),
        self(q->rootObjectHolder(), "self"),
        continuousSync(q->rootObjectHolder(), "continuousSync"),
        deleteAvailable(q->rootObjectHolder(), "deleteAvailable")
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
    nx::vms::client::desktop::SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(
        parent,
        dialogType == editGroup
            ? "Nx/Dialogs/UserManagement/GroupEditDialog.qml"
            : "Nx/Dialogs/UserManagement/GroupCreateDialog.qml"),
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

    if (dialogType == DialogType::editGroup)
    {
        connect(rootObjectHolder()->object(), SIGNAL(addGroupRequested()),
            this, SLOT(onAddGroupRequested()));

        connect(systemContext->nonEditableUsersAndGroups(),
            &NonEditableUsersAndGroups::groupModified,
            this,
            [this](const QnUuid& groupId)
            {
                if (d->groupId != groupId)
                    return;

                d->deleteAvailable =
                    !this->systemContext()->nonEditableUsersAndGroups()->containsGroup(groupId);
            });
    }

    if (dialogType == editGroup)
    {
        connect(rootObjectHolder()->object(), SIGNAL(deleteRequested()),
            this, SLOT(onDeleteRequested()));

        connect(globalSettings(), &common::SystemSettings::ldapSettingsChanged, this,
            [this]()
            {
                d->continuousSync = globalSettings()->ldap().continuousSync
                    == nx::vms::api::LdapSettings::Sync::usersAndGroups;
            });

        d->continuousSync = globalSettings()->ldap().continuousSync
            == nx::vms::api::LdapSettings::Sync::usersAndGroups;

        connect(systemContext->accessSubjectHierarchy(),
            &nx::core::access::SubjectHierarchy::changed,
            this,
            [this](
                const QSet<QnUuid>& /*added*/,
                const QSet<QnUuid>& /*removed*/,
                const QSet<QnUuid>& groupsWithChangedMembers,
                const QSet<QnUuid>& /*subjectsWithChangedParents*/)
            {
                if (groupsWithChangedMembers.contains(d->groupId))
                    updateStateFrom(d->groupId);
            });
    }

    connect(systemContext->userGroupManager(), &common::UserGroupManager::addedOrUpdated, this,
        [this](const nx::vms::api::UserGroupData& userGroup)
        {
            if (userGroup.id == d->groupId)
                updateStateFrom(userGroup.id);
        });

    connect(systemContext->userGroupManager(), &common::UserGroupManager::removed, this,
        [this](const nx::vms::api::UserGroupData& userGroup)
        {
            if (userGroup.id == d->groupId)
            {
                reject();
                setGroup({}); //< reject() will not clear the group when the dialog is closed.
            }
        });

    connect(
        systemContext->accessRightsManager(),
        &nx::core::access::AbstractAccessRightsManager::ownAccessRightsChanged,
        this,
        [this](const QSet<QnUuid>& subjectIds)
        {
            if (subjectIds.contains(d->groupId))
                updateStateFrom(d->groupId);
        });

    // This is needed only at apply, because reject and accept clear current group.
    connect(this, &QmlDialogWrapper::applied, this,
        [this]()
        {
            if (d->editingContext)
                d->editingContext.value()->revert();
        });

    connect(this, &QmlDialogWrapper::rejected, this, [this]() { setGroup({}); });
    connect(this, &QmlDialogWrapper::accepted, this, [this]() { setGroup({}); });
}

GroupSettingsDialog::~GroupSettingsDialog()
{
}

QString GroupSettingsDialog::validateName(const QString& text)
{
    const auto name = text.trimmed().toLower();

    if (name.isEmpty())
        return tr("Group name cannot be empty");

    for (const auto& group: systemContext()->userGroupManager()->groups())
    {
        if (!d->groupId.isNull() && d->groupId == group.id)
            continue;

        if (group.name.trimmed().toLower() == name)
            return tr("Group with the same name already exists");
    }

    return {};
}

void GroupSettingsDialog::onAddGroupRequested()
{
    d->sessionNotifier->actionManager()->trigger(ui::action::UserGroupsAction,
        ui::action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer(window())));
}

void GroupSettingsDialog::onDeleteRequested()
{
    if (!ui::messages::UserGroups::removeGroups(
        d->parentWidget,
        {d->groupId},
        /*allowSilent*/ false))
    {
        return;
    }

    d->isSaving = true;

    const auto handleRemove = nx::utils::guarded(this,
        [this](bool success, const QString& errorString)
        {
            d->isSaving = false;
            if (success)
            {
                reject();
                return;
            }

            QnMessageBox messageBox(
                QnMessageBoxIcon::Critical,
                tr("Delete failed"),
                errorString,
                QDialogButtonBox::Ok,
                QDialogButtonBox::Ok,
                d->parentWidget);
            messageBox.exec();
        });

    removeGroups(systemContext(), {d->groupId}, std::move(handleRemove));
}

bool GroupSettingsDialog::setGroup(const QnUuid& groupId)
{
    if (!d->groupId.isNull() && d->groupId == groupId)
        return true; //< Do not reset state upon setting the same group.

    if (!d->groupId.isNull() && !groupId.isNull() && isModified())
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

    d->groupId = groupId;

    if (groupId.isNull())
        d->tabIndex = 0;

    d->isSaving = false;
    createStateFrom(groupId);

    return true;
}

GroupSettingsDialogState GroupSettingsDialog::createState(const QnUuid& groupId)
{
    GroupSettingsDialogState state;

    const auto groupManager = systemContext()->userGroupManager();

    if (d->dialogType == createGroup)
    {
        QStringList usedNames;
        for (const auto& group: groupManager->groups())
            usedNames << group.name;

        state.name = nx::utils::generateUniqueString(
            usedNames, tr("New Group"), tr("New Group %1"));
        state.groupId = QnUuid::createUuid();
    }
    else
    {
        const auto groupData = groupManager->find(groupId);
        if (!groupData) //< Theoretically a race condition update-delete can happen.
            return state;

        state.name = groupData->name;
        state.groupId = groupId;
        state.description = groupData->description;
        state.globalPermissions = groupData->permissions;

        state.sharedResources = systemContext()->accessRightsManager()->ownResourceAccessMap(
            groupId);
        state.isLdap = (groupData->type == nx::vms::api::UserType::ldap);

        state.nameEditable = !state.isLdap
            && systemContext()->accessController()->hasPermissions(
                state.groupId,
                Qn::WriteNamePermission);

        state.descriptionEditable = systemContext()->accessController()->hasPermissions(
            state.groupId, Qn::SavePermission);

        state.isPredefined = groupData->attributes.testFlag(nx::vms::api::UserAttribute::readonly);

        for (const auto& parentGroupId: groupData->parentGroupIds)
            state.parentGroups.insert(MembersModelGroup::fromId(systemContext(), parentGroupId));

        const auto subjectHierarchy = systemContext()->accessSubjectHierarchy();
        const auto memberIds = subjectHierarchy->directMembers(groupId);

        QList<QnUuid> users;

        for (const auto& memberId: memberIds)
        {
            const auto subject = subjectHierarchy->subjectById(memberId);
            if (subject.isUser())
                users.push_back(memberId);
            else if (subject.isRole())
                state.groups.insert(memberId);
        }

        std::sort(users.begin(), users.end());
        state.users = users;

        state.permissionsEditable = systemContext()->accessController()->hasPermissions(
            state.groupId,
            Qn::WriteAccessRightsPermission);

        const auto accessController = qobject_cast<AccessController*>(
            systemContext()->accessController());

        state.membersEditable = !state.isLdap
            && NX_ASSERT(accessController)
            && accessController->canCreateUser(
                /*targetPermissions*/ {},
                /*targetGroups*/ {groupId});

        state.parentGroupsEditable = systemContext()->accessController()->hasPermissions(
            state.groupId,
            Qn::WriteAccessRightsPermission);

        d->deleteAvailable = !systemContext()->nonEditableUsersAndGroups()->containsGroup(groupId);

        state.nameIsUnique = true;
        const auto groupName = groupData->name.toLower();
        for (const auto& group: groupManager->groups())
        {
            if (group.getId() != groupId && group.name.toLower() == groupName)
            {
                state.nameIsUnique = false;
                break;
            }
        }
    }

    return state;
}

void GroupSettingsDialog::saveState(const GroupSettingsDialogState& state)
{
    if (d->dialogType == editGroup && !isModified())
    {
        saveStateComplete(state);
        return;
    }

    if (d->hasCycles(state))
    {
        QnMessageBox messageBox(
            QnMessageBoxIcon::Critical,
            tr("Failed to apply changes to %1 group").arg(state.name),
            "",
            QDialogButtonBox::Ok,
            QDialogButtonBox::Ok,
            d->parentWidget);
        messageBox.setWindowTitle(qApp->applicationDisplayName());
        messageBox.exec();
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

    const auto sharedResources = state.sharedResources.asKeyValueRange();
    updateData.groupData.resourceAccessRights = {sharedResources.begin(), sharedResources.end()};

    const auto usersDiff = differences(originalState().users.list(), state.users.list());
    const auto groupsDiff = differences(originalState().groups, state.groups);

    const auto resourcePool = QPointer<QnResourcePool>(systemContext()->resourcePool());

    auto chain = new UserGroupRequestChain(systemContext(), /*parent*/ this);

    // Remove group from groups.

    for (const auto& id: groupsDiff.removed)
    {
        if (const auto userGroup = systemContext()->userGroupManager()->find(id))
        {
            UserGroupRequest::ModifyGroupParents mod;
            mod.id = id;
            mod.prevParents = userGroup->parentGroupIds;
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
            mod.prevParents = user->groupIds();
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
        if (const auto userGroup = systemContext()->userGroupManager()->find(id))
        {
            UserGroupRequest::ModifyGroupParents mod;
            mod.id = id;
            mod.prevParents = userGroup->parentGroupIds;
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
            mod.prevParents = user->groupIds();
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

    d->isSaving = true;

    chain->start(nx::utils::guarded(this,
        [chain, state, this](
            bool success,
            const QString& errorString)
        {
            d->isSaving = false;

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
    nx::utils::MoveOnlyFunc<void(bool, const QString&)> callback)
{
    auto chain = new UserGroupRequestChain(systemContext);

    // Remove groups from groups.

    for (const auto& group: systemContext->userGroupManager()->groups())
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
        mod.prevParents = user->groupIds();
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
            const QString& errorString)
        {
            if (callback)
                callback(success, errorString);
            chain->deleteLater();
        }));
}

} // namespace nx::vms::client::desktop

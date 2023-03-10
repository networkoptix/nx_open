// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group_settings_dialog.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/algorithm/diff_sorted_lists.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>

#include "../globals/results_reporter.h"
#include "../globals/session_notifier.h"

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

void GroupSettingsDialog::removeGroups(
    nx::vms::client::desktop::SystemContext* systemContext,
    const QSet<QnUuid>& idsToRemove,
    std::function<void(bool)> callback)
{
    auto resultsReporter = ResultsReporter::create(
        [callback = std::move(callback), idsToRemove, systemContext](bool success)
        {
            if (success)
            {
                for (const auto& id: idsToRemove)
                    qnResourcesChangesManager->removeUserRole(id, systemContext);
            }
            if (callback)
                callback(success);
        });

    // Delete the group from existing users and groups.

    const auto handleUserSaved =
        [resultsReporter](bool success, const QnUserResourcePtr&)
        {
            resultsReporter->add(success);
        };

    const auto applyRemove =
        [idsToRemove](const QnUserResourcePtr& user)
        {
            std::vector<QnUuid> ids = user->userRoleIds();

            ids.erase(
                std::remove_if(
                    ids.begin(),
                    ids.end(),
                    [&idsToRemove](auto id){ return idsToRemove.contains(id); }),
                ids.end());

            user->setUserRoleIds(ids);
        };

    for (const auto& user: systemContext->resourcePool()->getResources<QnUserResource>())
    {
        const auto groupIds = user->userRoleIds();

        const auto it = std::find_if(
            groupIds.begin(),
            groupIds.end(),
            [&idsToRemove](auto id){ return idsToRemove.contains(id); });

        if (it != groupIds.end())
        {
            qnResourcesChangesManager->saveUser(user,
                QnUserResource::DigestSupport::keep,
                applyRemove,
                systemContext,
                handleUserSaved);
        }
    }

    auto handleGroupSaved =
        [resultsReporter](
            bool roleIsStored,
            const api::UserRoleData&)
        {
            resultsReporter->add(roleIsStored);
        };

    for (auto group: systemContext->userRolesManager()->userRoles())
    {
        const auto last = group.parentGroupIds.end();
        const auto it = group.parentGroupIds.erase(
            std::remove_if(
                group.parentGroupIds.begin(),
                last,
                [&idsToRemove](auto id){ return idsToRemove.contains(id); }),
            last);

        if (it == last) // Nothing was removed.
            continue;

        qnResourcesChangesManager->saveUserRole(group, systemContext, handleGroupSaved);
    }
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
    api::UserRoleData groupData;

    if (d->dialogType == createGroup)
        groupData.id = state.groupId;
    else
        groupData = systemContext()->userRolesManager()->userRole(d->groupId);

    groupData.name = state.name;
    groupData.description = state.description;
    groupData.parentGroupIds.clear();
    for (const auto& group: state.parentGroups)
        groupData.parentGroupIds.push_back(group.id);

    if (d->hasCycles(state))
    {
        // TODO: show a banner about cycles.
        return;
    }

    groupData.permissions = state.globalPermissions;

    const auto usersDiff = differences(originalState().users.list(), state.users.list());
    const auto groupsDiff = differences(originalState().groups, state.groups);

    auto resultsReporter = ResultsReporter::create(
        nx::utils::guarded(this,
            [this, state](bool success)
            {
                if (success)
                    saveStateComplete(state);
            }));

    const auto resourcePool = QPointer<QnResourcePool>(systemContext()->resourcePool());

    if (!groupData.isPredefined)
    {
        auto saveAccessRightsCallback = nx::utils::guarded(this,
            [resultsReporter](bool ok)
            {
                resultsReporter->add(ok);
            });

        auto setupAccessibleResources = nx::utils::guarded(this,
            [this, saveAccessRightsCallback, resultsReporter, state, resourcePool](
                bool roleIsStored,
                const api::UserRoleData& group)
            {
                resultsReporter->add(roleIsStored);

                qnResourcesChangesManager->saveAccessRights(
                    group, state.sharedResources, saveAccessRightsCallback, systemContext());
            });

        qnResourcesChangesManager->saveUserRole(
            groupData, systemContext(), setupAccessibleResources);
    }

    // There is no server API call to update the whole hierarchy with a single call and intermidiate
    // hierarchy state may contain cycles. To prevent saving cycles to the server, handle addition
    // of users/groups only after all remove opertaions have succeeded.
    auto removeReporter = ResultsReporter::create(
        nx::utils::guarded(this,
            [this, resultsReporter, resourcePool, state, usersDiff, groupsDiff, groupData](
                bool success)
            {
                if (!success)
                    return;

                // Add groups.

                const auto handleGroupAdded = nx::utils::guarded(this,
                    [resultsReporter](bool roleIsStored, const api::UserRoleData&)
                    {
                        resultsReporter->add(roleIsStored);
                    });

                for (const auto& id: groupsDiff.added)
                {
                    auto userGroup = systemContext()->userRolesManager()->userRole(id);
                    userGroup.parentGroupIds.push_back(groupData.id);
                    qnResourcesChangesManager->saveUserRole(
                        userGroup, systemContext(), handleGroupAdded);
                }

                // Add users.

                const auto handleUserAdded = nx::utils::guarded(this,
                    [resultsReporter](bool success, const QnUserResourcePtr&)
                    {
                        resultsReporter->add(success);
                    });

                const auto applyAdd = nx::utils::guarded(this,
                    [groupId = groupData.id](const QnUserResourcePtr& user)
                    {
                        std::vector<QnUuid> ids = user->userRoleIds();
                        ids.push_back(groupId);
                        user->setUserRoleIds(ids);
                    });

                for (const auto& userId: usersDiff.added)
                {
                    if (auto user = resourcePool->getResourceById<QnUserResource>(userId))
                    {
                        qnResourcesChangesManager->saveUser(user,
                            QnUserResource::DigestSupport::keep,
                            applyAdd,
                            systemContext(),
                            handleUserAdded);
                    }
                }
            }));

    // Remove groups.

    const auto handleGroupRemoved = nx::utils::guarded(this,
        [removeReporter](bool roleIsStored, const api::UserRoleData&)
        {
            removeReporter->add(roleIsStored);
        });

    for (const auto& id: groupsDiff.removed)
    {
        auto userGroup = systemContext()->userRolesManager()->userRole(id);

        userGroup.parentGroupIds.erase(
            std::remove(
                userGroup.parentGroupIds.begin(),
                userGroup.parentGroupIds.end(),
                groupData.id),
            userGroup.parentGroupIds.end());

        qnResourcesChangesManager->saveUserRole(userGroup, systemContext(), handleGroupRemoved);
    }

    // Remove users.

    const auto handleUserRemoved = nx::utils::guarded(this,
        [removeReporter](bool success, const QnUserResourcePtr&)
        {
            removeReporter->add(success);
        });

    const auto applyRemove = nx::utils::guarded(this,
        [groupId = groupData.id](const QnUserResourcePtr& user)
        {
            std::vector<QnUuid> ids = user->userRoleIds();
            ids.erase(std::remove(ids.begin(), ids.end(), groupId), ids.end());
            user->setUserRoleIds(ids);
        });

    for (const auto& userId: usersDiff.removed)
    {
        if (auto user = resourcePool->getResourceById<QnUserResource>(userId))
        {
            qnResourcesChangesManager->saveUser(user,
                QnUserResource::DigestSupport::keep,
                applyRemove,
                systemContext(),
                handleUserRemoved);
        }
    }
}

} // namespace nx::vms::client::desktop

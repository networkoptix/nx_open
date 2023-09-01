// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings_dialog_manager.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_administration/dialogs/group_settings_dialog.h>
#include <nx/vms/client/desktop/system_administration/dialogs/user_settings_dialog.h>

namespace nx::vms::client::desktop {

class SettingsDialogManager::Private
{
public:
    QnUuid currentUserId;
    QPointer<UserSettingsDialog> userSettingsDialog;

    QnUuid currentGroupId;
    QPointer<GroupSettingsDialog> groupSettingsDialog;
};

SettingsDialogManager::SettingsDialogManager(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    d(new Private())
{
}

SettingsDialogManager::~SettingsDialogManager()
{
}

QnUuid SettingsDialogManager::currentEditedUserId() const
{
    return d->currentUserId;
}

void SettingsDialogManager::setCurrentEditedUserId(const QnUuid& userId)
{
    if (!d->userSettingsDialog)
    {
        if (d->currentUserId == userId)
            return;

        d->currentUserId = userId;
        emit currentEditedUserIdChanged();

        return;
    }

    const auto user = resourcePool()->getResourceById<QnUserResource>(userId);
    if (!d->userSettingsDialog->setUser(user))
    {
        emit currentEditedUserIdChangeFailed();
        return;
    }

    d->currentUserId = userId;
    emit currentEditedUserIdChanged();
}

void SettingsDialogManager::editUser(const QnUuid& userId, int tab, QWidget* parent)
{
    setCurrentEditedUserId(userId);

    if (!parent)
        parent = mainWindowWidget();

    if (!d->userSettingsDialog)
    {
        d->userSettingsDialog = new UserSettingsDialog(
            UserSettingsDialog::DialogType::EditUser, systemContext(), parent);
        connect(d->userSettingsDialog.get(), &UserSettingsDialog::done, this,
            [this]() { setCurrentEditedUserId({}); });
        d->userSettingsDialog->setUser(resourcePool()->getResourceById<QnUserResource>(userId));
    }

    d->userSettingsDialog->setTransientParent(parent);

    if (tab >= 0)
        d->userSettingsDialog->selectTab((UserSettingsDialog::Tab) tab);

    d->userSettingsDialog->open();
    d->userSettingsDialog->raise();
}

void SettingsDialogManager::createUser(QWidget* parent)
{
    auto dialog = std::make_unique<UserSettingsDialog>(
        UserSettingsDialog::DialogType::CreateUser, systemContext(), parent);
    dialog->setUser({});
    dialog->exec(Qt::ApplicationModal);
}

QnUuid SettingsDialogManager::currentEditedGroupId() const
{
    return d->currentGroupId;
}

void SettingsDialogManager::setCurrentEditedGroupId(const QnUuid& groupId)
{
    if (!d->groupSettingsDialog)
    {
        if (d->currentGroupId == groupId)
            return;

        d->currentGroupId = groupId;
        emit currentEditedGroupIdChanged();

        return;
    }

    if (!d->groupSettingsDialog->setGroup(groupId))
    {
        emit currentEditedGroupIdChangeFailed();
        return;
    }

    d->currentGroupId = groupId;
    emit currentEditedGroupIdChanged();
}

void SettingsDialogManager::editGroup(const QnUuid& groupId, QWidget* parent)
{
    setCurrentEditedGroupId(groupId);

    if (!d->groupSettingsDialog)
    {
        d->groupSettingsDialog = new GroupSettingsDialog(
            GroupSettingsDialog::editGroup, systemContext(), parent);
        connect(d->groupSettingsDialog.get(), &GroupSettingsDialog::done, this,
            [this]() { setCurrentEditedGroupId({}); });
        d->groupSettingsDialog->setGroup(groupId);
    }

    d->groupSettingsDialog->setTransientParent(parent);

    d->groupSettingsDialog->open();
    d->groupSettingsDialog->raise();
}

void SettingsDialogManager::createGroup(QWidget* parent)
{
    auto dialog = std::make_unique<GroupSettingsDialog>(
        GroupSettingsDialog::createGroup, systemContext(), parent);
    dialog->setGroup({});
    dialog->exec(Qt::ApplicationModal);
}

} // namespace nx::vms::client::desktop

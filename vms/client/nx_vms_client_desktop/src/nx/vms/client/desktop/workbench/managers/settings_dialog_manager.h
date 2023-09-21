// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QWidget;

namespace nx::vms::client::desktop {

/**
 * This class manages globally available single instance settings dialogs. Currently it supports
 * dialogs for editing user and group settings.
 */
class SettingsDialogManager:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    SettingsDialogManager(QObject* parent);
    virtual ~SettingsDialogManager() override;

    /**
     * Returns an ID of the user which is currently opened in the user settings dialog. If the
     * dialog has not been created yet, it returns the latest ID set by setCurrentEditedUserId.
     */
    QnUuid currentEditedUserId() const;
    /**
     * Opens the specified user in the user settings dialog. If the dialog has not been created
     * yet, simply stores the value. The opened dialog is not brought to front and not activated.
     */
    void setCurrentEditedUserId(const QnUuid& userId);
    /**
     * Opens the specified user in the user settings dialog.
     */
    void editUser(const QnUuid& userId, int tab = -1, QWidget* parent = nullptr);

    bool isEditUserDialogVisible() const;

    /**
     * Opens a user creation dialog. Multiple instances could be opened simultaneously. That does
     * not affect the user settings dialog and does not affect the currentEditedUserId().
     */
    void createUser(QWidget* parent = nullptr);

    /**
     * Returns an ID of the group which is currently opened in the group settings dialog. If the
     * dialog has not been created yet, it returns the latest ID set by setCurrentEditedGroupId.
     */
    QnUuid currentEditedGroupId() const;
    /**
     * Opens the specified group in the group settings dialog. If the dialog has not been created
     * yet, simply stores the value. The opened dialog is not brought to front and not activated.
     */
    void setCurrentEditedGroupId(const QnUuid& groupId);
    /**
     * Opens the specified group in the group settings dialog. If the dialog has not been created
     * yet, simply stores the value. The opened dialog is not brought to front and not activated.
     */
    void editGroup(const QnUuid& groupId, QWidget* parent = nullptr);

    bool isEditGroupDialogVisible() const;

    /**
     * Opens the specified group in the group settings dialog.
     */
    void createGroup(QWidget* parent = nullptr);

signals:
    /**
     * Emitted when currentEditedUserId() is changed either by setCurrentEditedUserId() or by
     * editUser().
     */
    void currentEditedUserIdChanged();
    /**
     * Emitted when the user settings dialog failed to switch the current user. This may happen
     * when there are unsaved changes in the dialog and the user declined to discard them.
     */
    void currentEditedUserIdChangeFailed();
    /**
     * Emitted when currentEditedGroupId() is changed either by setCurrentEditedGroupId() or by
     * editGroup().
     */
    void currentEditedGroupIdChanged();
    /**
     * Emitted when the group settings dialog failed to switch the current group. This may happen
     * when there are unsaved changes in the dialog and the user declined to discard them.
     */
    void currentEditedGroupIdChangeFailed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop

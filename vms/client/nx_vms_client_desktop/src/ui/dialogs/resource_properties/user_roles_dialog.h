// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnUserRolesSettingsModel;

namespace Ui
{
class UserRolesDialog;
}

class QnUserRolesDialog: public QnSessionAwareTabbedDialog
{
    Q_OBJECT

    typedef QnSessionAwareTabbedDialog base_type;

public:
    enum DialogPage
    {
        SettingsPage,
        PermissionsPage,
        CamerasPage,
        LayoutsPage,

        PageCount
    };

    QnUserRolesDialog(QWidget* parent = nullptr);
    virtual ~QnUserRolesDialog();

    bool selectUserRole(const QnUuid& userRoleId);

protected:
    virtual bool canApplyChanges() const override;
    virtual bool hasChanges() const override;
    virtual void applyChanges() override;

private:
    void at_model_currentChanged(const QModelIndex& current);

private:
    Q_DISABLE_COPY(QnUserRolesDialog)

    QScopedPointer<Ui::UserRolesDialog> ui;

    /** Common model for all sub-widgets, owned by the dialog. */
    QnUserRolesSettingsModel* m_model;

    using SharedResourcesByRoleId = QHash<QnUuid, QSet<QnUuid>>;
    SharedResourcesByRoleId m_sharedResourcesQueuedToSave;
};

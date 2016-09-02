#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

class QnUserGroupSettingsWidget;
class QnPermissionsWidget;
class QnAccessibleResourcesWidget;
class QnAbstractPermissionsModel;
class QnUserGroupSettingsModel;

namespace Ui
{
    class UserGroupsDialog;
}

class QnUserGroupsDialog: public QnSessionAwareTabbedDialog
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

    QnUserGroupsDialog(QWidget* parent = NULL);
    virtual ~QnUserGroupsDialog();

    bool selectGroup(const QnUuid& groupId);

protected:
    virtual bool hasChanges() const override;
    virtual void applyChanges() override;

private:
    Q_DISABLE_COPY(QnUserGroupsDialog)

    QScopedPointer<Ui::UserGroupsDialog> ui;

    /** Common model for all sub-widgets, owned by the dialog. */
    QnUserGroupSettingsModel* m_model;

    QnUserGroupSettingsWidget* m_settingsPage;
    QnPermissionsWidget* m_permissionsPage;
    QnAccessibleResourcesWidget* m_camerasPage;
    QnAccessibleResourcesWidget* m_layoutsPage;
};



#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnUserGroupSettingsWidget;
class QnPermissionsWidget;
class QnAccessibleResourcesWidget;
class QnAbstractPermissionsModel;
class QnUserGroupSettingsModel;

namespace Ui
{
    class UserGroupsDialog;
}

class QnUserGroupsDialog: public QnWorkbenchStateDependentTabbedDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentTabbedDialog base_type;

public:
    enum DialogPage
    {
        SettingsPage,
        PermissionsPage,
        CamerasPage,
        LayoutsPage,
        ServersPage,

        PageCount
    };

    QnUserGroupsDialog(QWidget *parent = NULL);
    virtual ~QnUserGroupsDialog();

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
    QnAccessibleResourcesWidget* m_serversPage;

};



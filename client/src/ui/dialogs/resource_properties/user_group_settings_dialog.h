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
    class UserGroupSettingsDialog;
}

class QnUserGroupSettingsDialog: public QnWorkbenchStateDependentTabbedDialog
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

    QnUserGroupSettingsDialog(QWidget *parent = NULL);
    virtual ~QnUserGroupSettingsDialog();

    QnUuid userGroupId() const;
    void setUserGroupId(const QnUuid& value);

protected:
    virtual void retranslateUi() override;

    virtual bool hasChanges() const override;
    virtual void applyChanges() override;

private:
    Q_DISABLE_COPY(QnUserGroupSettingsDialog)

    QScopedPointer<Ui::UserGroupSettingsDialog> ui;

    /** Common model for all sub-widgets, owned by the dialog. */
    QnUserGroupSettingsModel* m_model;

    QnUserGroupSettingsWidget* m_settingsPage;
    QnPermissionsWidget* m_permissionsPage;
    QnAccessibleResourcesWidget* m_camerasPage;
    QnAccessibleResourcesWidget* m_layoutsPage;
    QnAccessibleResourcesWidget* m_serversPage;

};



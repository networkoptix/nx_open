#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnUserSettingsWidget;
class QnUserAccessRightsResourcesWidget;

namespace Ui
{
    class UserSettingsDialog;
}

class QnUserSettingsDialog: public QnWorkbenchStateDependentTabbedDialog
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

    QnUserSettingsDialog(QWidget *parent = NULL);
    virtual ~QnUserSettingsDialog();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr &user);

protected:
    virtual QDialogButtonBox::StandardButton showConfirmationDialog() override;
    virtual void retranslateUi() override;

    virtual void applyChanges() override;

private:
    Q_DISABLE_COPY(QnUserSettingsDialog)

    QScopedPointer<Ui::UserSettingsDialog> ui;
    QnUserResourcePtr m_user;

    QnUserSettingsWidget* m_settingsPage;
    QnUserAccessRightsResourcesWidget* m_camerasPage;
    QnUserAccessRightsResourcesWidget* m_layoutsPage;
    QnUserAccessRightsResourcesWidget* m_serversPage;

};



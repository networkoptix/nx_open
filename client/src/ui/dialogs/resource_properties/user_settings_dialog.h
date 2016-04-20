#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnUserSettingsWidget;
class QnPermissionsWidget;
class QnAccessibleResourcesWidget;
class QnAbstractPermissionsDelegate;

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
    QScopedPointer<QnAbstractPermissionsDelegate> m_permissionsDelegate;

    QnUserResourcePtr m_user;

    QnUserSettingsWidget* m_settingsPage;
    QnPermissionsWidget* m_permissionsPage;
    QnAccessibleResourcesWidget* m_camerasPage;
    QnAccessibleResourcesWidget* m_layoutsPage;
    QnAccessibleResourcesWidget* m_serversPage;

};



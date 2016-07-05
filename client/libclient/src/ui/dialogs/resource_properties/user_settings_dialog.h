#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnUserProfileWidget;
class QnUserSettingsWidget;
class QnPermissionsWidget;
class QnAccessibleResourcesWidget;
class QnAbstractPermissionsModel;
class QnUserSettingsModel;

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
        ProfilePage,
        SettingsPage,
        PermissionsPage,
        CamerasPage,
        LayoutsPage,

        PageCount
    };

    QnUserSettingsDialog(QWidget *parent = NULL);
    virtual ~QnUserSettingsDialog();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr &user);

protected:
    virtual QDialogButtonBox::StandardButton showConfirmationDialog() override;
    virtual void retranslateUi() override;

    virtual bool hasChanges() const override;
    virtual void applyChanges() override;

private:
    void updateControlsVisibility();
    void permissionsChanged();

private:
    Q_DISABLE_COPY(QnUserSettingsDialog)

    QScopedPointer<Ui::UserSettingsDialog> ui;

    /** Common model for all sub-widgets, owned by the dialog. */
    QnUserSettingsModel* m_model;

    QnUserResourcePtr m_user;

    QnUserProfileWidget* m_profilePage;
    QnUserSettingsWidget* m_settingsPage;
    QnPermissionsWidget* m_permissionsPage;
    QnAccessibleResourcesWidget* m_camerasPage;
    QnAccessibleResourcesWidget* m_layoutsPage;
    QPushButton* m_editGroupsButton;

};



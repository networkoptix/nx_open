#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

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

class QnUserSettingsDialog: public QnSessionAwareTabbedDialog
{
    Q_OBJECT

    typedef QnSessionAwareTabbedDialog base_type;

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

    void forcedUpdate();

protected:
    virtual QDialogButtonBox::StandardButton showConfirmationDialog() override;
    virtual void retranslateUi() override;
    virtual void applyChanges() override;

private:
    void applyChangesInternal();
    void updateControlsVisibility();
    void updatePermissions();

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

Q_DECLARE_METATYPE(QnUserSettingsDialog::DialogPage)

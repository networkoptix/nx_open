#include "camera_settings_dialog.h"
#include "ui_camera_settings_dialog.h"

#include <QtWidgets/QPushButton>

#include <client_core/connection_context_aware.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/user_resource.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/html.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>

namespace nx {
namespace client {
namespace desktop {

CameraSettingsDialog::CameraSettingsDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::CameraSettingsDialog())
{
    ui->setupUi(this);
/*
    addPage(QnUserSettingsDialog::ProfilePage, m_profilePage, tr("User Information"));
    addPage(SettingsPage, m_settingsPage, tr("User Information"));
    addPage(PermissionsPage, m_permissionsPage, tr("Permissions"));
    addPage(CamerasPage, m_camerasPage, tr("Cameras && Resources"));
    addPage(LayoutsPage, m_layoutsPage, tr("Layouts"));

    connect(resourceAccessManager(), &QnResourceAccessManager::permissionsChanged, this,
        [this](const QnResourceAccessSubject& subject)
        {
            if (m_user && subject.user() == m_user)
                updatePermissions();
        });

    connect(m_settingsPage, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &CameraSettingsDialog::updatePermissions);
    connect(m_permissionsPage, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &CameraSettingsDialog::updatePermissions);
    connect(m_camerasPage, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &CameraSettingsDialog::updatePermissions);
    connect(m_layoutsPage, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &CameraSettingsDialog::updatePermissions);

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this, [this](const QnResourceList &resources)
    {
        if (isHidden())
            return;

        auto users = resources.filtered<QnUserResource>();
        if (!users.isEmpty())
            setUser(users.first());
    });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr& resource)
    {
        if (resource != m_user)
        {
            // TODO: #vkutin #GDM check if permissions change is correctly handled through a chain of signals
            return;
        }
        setUser(QnUserResourcePtr());
        tryClose(true);
    });

    ui->alertBar->setReservedSpace(false);

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    // Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code.
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);
    */
}

CameraSettingsDialog::~CameraSettingsDialog()
{
}

bool CameraSettingsDialog::setCameras(const QnVirtualCameraResourceList& cameras, bool force)
{
    return true;

}
/*
QnUserResourcePtr CameraSettingsDialog::user() const
{
    return m_user;
}

void CameraSettingsDialog::setUser(const QnUserResourcePtr &user)
{
    if (m_user == user)
        return;

    if (!tryClose(false))
        return;

    if (m_user)
        m_user->disconnect(this);

    m_user = user;
    m_model->setUser(user);

    if (m_user)
    {
        connect(m_user, &QnResource::propertyChanged, this,
            [this](const QnResourcePtr& resource, const QString& propertyName)
            {
                if (resource == m_user && propertyName == cloudAuthInfoPropertyName)
                    forcedUpdate();
            });
    }

    // Hide Apply button if cannot apply changes.
    bool applyButtonVisible = m_model->mode() == QnUserSettingsModel::OwnProfile
                           || m_model->mode() == QnUserSettingsModel::OtherSettings;
    ui->buttonBox->button(QDialogButtonBox::Apply)->setVisible(applyButtonVisible);

    // Hide Cancel button if we cannot edit user.
    bool cancelButtonVisible = m_model->mode() != QnUserSettingsModel::OtherProfile
                            && m_model->mode() != QnUserSettingsModel::Invalid;
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(cancelButtonVisible);

    forcedUpdate();
}

void CameraSettingsDialog::loadDataToUi()
{
    ui->alertBar->setText(QString());

    base_type::loadDataToUi();

    if (!m_user)
        return;

    bool userIsEnabled = m_user->isEnabled();
    m_userEnabledButton->setChecked(userIsEnabled);

    if (m_user->userType() == QnUserType::Cloud
        && m_model->mode() == QnUserSettingsModel::OtherSettings)
    {
        const auto auth = m_user->getProperty(cloudAuthInfoPropertyName);
        if (auth.isEmpty())
        {
            ui->alertBar->setText(tr("This user has not yet signed up for %1",
                "%1 is the cloud name (like 'Nx Cloud')").arg(nx::network::AppInfo::cloudName()));
            return;
        }
    }
    if (!userIsEnabled)
        ui->alertBar->setText(tr("User is disabled"));
}

void CameraSettingsDialog::forcedUpdate()
{
    Qn::updateGuarded(this, [this]() { base_type::forcedUpdate(); });
    updatePermissions();
    updateButtonBox();
}

QDialogButtonBox::StandardButton CameraSettingsDialog::showConfirmationDialog()
{
    NX_ASSERT(m_user, Q_FUNC_INFO, "User must exist here");

    if (m_model->mode() != QnUserSettingsModel::OwnProfile && m_model->mode() != QnUserSettingsModel::OtherSettings)
        return QDialogButtonBox::Cancel;

    if (!canApplyChanges())
        return QDialogButtonBox::Cancel;

    const auto result = QnMessageBox::question(this,
        tr("Apply changes before switching to another user?"), QString(),
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply);

    if (result == QDialogButtonBox::Apply)
        return QDialogButtonBox::Yes;
    if (result == QDialogButtonBox::Discard)
        return QDialogButtonBox::No;

    return QDialogButtonBox::Cancel;
}

void CameraSettingsDialog::retranslateUi()
{
    base_type::retranslateUi();
    if (!m_user)
        return;

    if (m_model->mode() == QnUserSettingsModel::NewUser)
    {
        setWindowTitle(tr("New User..."));
        setHelpTopic(this, Qn::NewUser_Help);
    }
    else
    {
        bool readOnly = !accessController()->hasPermissions(m_user, Qn::WritePermission | Qn::SavePermission);
        setWindowTitle(readOnly
            ? tr("User Settings - %1 (readonly)").arg(m_user->getName())
            : tr("User Settings - %1").arg(m_user->getName()));

        setHelpTopic(this, Qn::UserSettings_Help);
    }
}

void CameraSettingsDialog::applyChanges()
{
    auto mode = m_model->mode();
    if (mode == QnUserSettingsModel::Invalid || mode == QnUserSettingsModel::OtherProfile)
        return;

    // TODO: #GDM #access SafeMode what to rollback if current password changes cannot be saved?

    // Handle new user creating.
    auto callbackFunction =
        [this, mode](bool success, const QnUserResourcePtr& user)
        {
            if (mode != QnUserSettingsModel::NewUser)
                return;

            // Cannot capture the resource directly because real resource pointer may differ if the
            // transaction is received before the request callback.
            NX_EXPECT(user);
            if (success && user)
                menu()->trigger(action::SelectNewItemAction, user);
        };

    qnResourcesChangesManager->saveUser(m_user, applyChangesFunction, callbackFunction);

    if (m_user->userRole() == Qn::UserRole::CustomPermissions)
    {
        auto accessibleResources = m_model->accessibleResources();

        QnLayoutResourceList layoutsToShare = resourcePool()->getResources(accessibleResources)
            .filtered<QnLayoutResource>(
                [](const QnLayoutResourcePtr& layout)
            {
                return !layout->isFile() && !layout->isShared();
            });

        for (const auto& layout : layoutsToShare)
        {
            menu()->trigger(action::ShareLayoutAction,
                action::Parameters(layout).withArgument(Qn::UserResourceRole, m_user));
        }

        qnResourcesChangesManager->saveAccessibleResources(m_user, accessibleResources);
    }
    else
    {
        qnResourcesChangesManager->cleanAccessibleResources(m_user->getId());
    }

    forcedUpdate();
}

void CameraSettingsDialog::applyChangesInternal()
{
    base_type::applyChanges();

    if (accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission))
        m_user->setEnabled(m_userEnabledButton->isChecked());
}

bool CameraSettingsDialog::hasChanges() const
{
    if (base_type::hasChanges())
        return true;

    if (m_user && accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission))
        return (m_user->isEnabled() != m_userEnabledButton->isChecked());

    return false;
}

void CameraSettingsDialog::updateControlsVisibility()
{
    auto mode = m_model->mode();

    // We are displaying profile for ourself and for users, that we cannot edit.
    bool profilePageVisible = mode == QnUserSettingsModel::OwnProfile
        || mode == QnUserSettingsModel::OtherProfile;

    bool settingsPageVisible = mode == QnUserSettingsModel::NewUser
        || mode == QnUserSettingsModel::OtherSettings;

    bool customAccessRights = settingsPageVisible
        && m_settingsPage->selectedRole() == Qn::UserRole::CustomPermissions;

    setPageVisible(ProfilePage,     profilePageVisible);

    setPageVisible(SettingsPage,    settingsPageVisible);
    setPageVisible(PermissionsPage, customAccessRights);
    setPageVisible(CamerasPage,     customAccessRights);
    setPageVisible(LayoutsPage,     customAccessRights);

    m_userEnabledButton->setVisible(settingsPageVisible && m_user
        && accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission));

    // Buttons state takes into account pages visibility, so we must recalculate it.
    updateButtonBox();
}
*/

} // namespace desktop
} // namespace client
} // namespace nx

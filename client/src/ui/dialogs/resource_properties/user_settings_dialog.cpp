#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/widgets/properties/user_settings_widget.h>
#include <ui/widgets/properties/accessible_resources_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/workbench_access_controller.h>

QnUserSettingsDialog::QnUserSettingsDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::UserSettingsDialog()),
    m_user(),
    m_settingsPage(new QnUserSettingsWidget(this)),
    m_permissionsPage(new QnPermissionsWidget(this)),
    m_camerasPage(new QnAccessibleResourcesWidget(QnAccessibleResourcesWidget::CamerasFilter, this)),
    m_layoutsPage(new QnAccessibleResourcesWidget(QnAccessibleResourcesWidget::LayoutsFilter, this)),
    m_serversPage(new QnAccessibleResourcesWidget(QnAccessibleResourcesWidget::ServersFilter, this))
{
    ui->setupUi(this);

    addPage(SettingsPage, m_settingsPage, tr("User Information"));
    addPage(PermissionsPage, m_permissionsPage, tr("Permissions"));
    addPage(CamerasPage, m_camerasPage, tr("Cameras"));
    addPage(LayoutsPage, m_layoutsPage, tr("Layouts"));
    addPage(ServersPage, m_serversPage, tr("Servers"));

    setPageVisible(PermissionsPage, false);
    setPageVisible(CamerasPage, false);
    setPageVisible(LayoutsPage, false);
    setPageVisible(ServersPage, false);

    connect(m_settingsPage, &QnAbstractPreferencesWidget::hasChangesChanged, this, [this]
    {
        bool customAccessRights = m_settingsPage->isCustomAccessRights();
        setPageVisible(PermissionsPage, customAccessRights);
        setPageVisible(CamerasPage, customAccessRights);
        setPageVisible(LayoutsPage, customAccessRights);
        setPageVisible(ServersPage, customAccessRights);
    });

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this, [this](const QnResourceList &resources)
    {
        if (isHidden())
            return;

        auto users = resources.filtered<QnUserResource>();
        if (!users.isEmpty())
            setUser(users.first());
    });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr& resource)
    {
        if (resource != m_user)
            return;
        setUser(QnUserResourcePtr());
        tryClose(true);
    });

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    //TODO: #GDM #access connect to resource pool to check if user was deleted

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);
}

QnUserSettingsDialog::~QnUserSettingsDialog()
{}


QnUserResourcePtr QnUserSettingsDialog::user() const
{
    return m_user;
}

void QnUserSettingsDialog::setUser(const QnUserResourcePtr &user)
{
    if(m_user == user)
        return;

    if (!tryClose(false))
        return;

    m_settingsPage->setUser(user);
    m_permissionsPage->setTargetUser(user);
    m_camerasPage->setTargetUser(user);
    m_layoutsPage->setTargetUser(user);
    m_serversPage->setTargetUser(user);

    m_user = user;

    /* Hide apply button. We are creating users one-by-one. */
    ui->buttonBox->button(QDialogButtonBox::Apply)->setVisible(m_user && !m_user->flags().testFlag(Qn::local));

    loadDataToUi();
    retranslateUi();
}

QDialogButtonBox::StandardButton QnUserSettingsDialog::showConfirmationDialog()
{
    NX_ASSERT(m_user, Q_FUNC_INFO, "User must exist here");

    if (m_user && m_user->flags().testFlag(Qn::local))
        return QDialogButtonBox::No;

    return QnMessageBox::question(
        this,
        tr("User not saved"),
        tr("Apply changes to user %1?")
        .arg(m_user
            ? m_user->getName()
            : QString()),
        QDialogButtonBox::Yes | QDialogButtonBox::No,
        QDialogButtonBox::Yes);
}

void QnUserSettingsDialog::retranslateUi()
{
    base_type::retranslateUi();
    if (m_user)
    {
        bool readOnly = !accessController()->hasPermissions(m_user, Qn::WritePermission | Qn::SavePermission);
        setWindowTitle(readOnly
            ? tr("User Settings - %1 (readonly)").arg(m_user->getName())
            : tr("User Settings - %1").arg(m_user->getName())
            );
        setHelpTopic(this, Qn::UserSettings_Help);
    }
    else
    {
        setWindowTitle(tr("New User..."));
        setHelpTopic(this, Qn::NewUser_Help);
    }
}

void QnUserSettingsDialog::applyChanges()
{
    base_type::applyChanges();
    if (m_user && m_user->flags().testFlag(Qn::local))
    {
        /* New User was created, clear dialog. */
        setUser(QnUserResourcePtr());
    }
}


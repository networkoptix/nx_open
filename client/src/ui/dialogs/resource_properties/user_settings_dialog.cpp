#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <core/resource/user_resource.h>

#include <ui/widgets/properties/user_settings_widget.h>
#include <ui/widgets/properties/user_access_rights_resources_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>

QnUserSettingsDialog::QnUserSettingsDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::UserSettingsDialog()),
    m_user(),
    m_settingsPage(new QnUserSettingsWidget(this)),
    m_camerasPage(new QnUserAccessRightsResourcesWidget(QnUserAccessRightsResourcesWidget::CamerasFilter, this)),
    m_layoutsPage(new QnUserAccessRightsResourcesWidget(QnUserAccessRightsResourcesWidget::LayoutsFilter, this)),
    m_serversPage(new QnUserAccessRightsResourcesWidget(QnUserAccessRightsResourcesWidget::ServersFilter, this))
{
    ui->setupUi(this);

    addPage(SettingsPage, m_settingsPage, tr("User Information"));
    addPage(CamerasPage, m_camerasPage, tr("Cameras"));
    addPage(LayoutsPage, m_layoutsPage, tr("Layouts"));
    addPage(ServersPage, m_serversPage, tr("Servers"));

    setPageVisible(CamerasPage, false);
    setPageVisible(LayoutsPage, false);
    setPageVisible(ServersPage, false);

    connect(m_settingsPage, &QnAbstractPreferencesWidget::hasChangesChanged, this, [this]
    {
        bool customAccessRights = m_settingsPage->isCustomAccessRights();
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

    QnUuid targetId = user ? user->getId() : QnUuid();

    m_settingsPage->setUser(user);
    m_camerasPage->setTargetId(targetId);
    m_layoutsPage->setTargetId(targetId);
    m_serversPage->setTargetId(targetId);

    m_user = user;

    loadDataToUi();
}

QDialogButtonBox::StandardButton QnUserSettingsDialog::showConfirmationDialog()
{
    NX_ASSERT(m_user, Q_FUNC_INFO, "User must exist here");

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


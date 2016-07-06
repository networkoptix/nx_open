#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/actions/action.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/actions/action_manager.h>
#include <ui/models/resource_properties/user_settings_model.h>
#include <ui/widgets/properties/user_profile_widget.h>
#include <ui/widgets/properties/user_settings_widget.h>
#include <ui/widgets/properties/accessible_resources_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>


namespace
{
    static std::map<QnResourceAccessFilter::Filter, QString> kCategoryNameByFilter
    {
        { QnResourceAccessFilter::CamerasFilter, QnUserSettingsDialog::tr("Media Resources") },
        { QnResourceAccessFilter::LayoutsFilter, QnUserSettingsDialog::tr("Layouts") }
    };

    static const QString kHtmlTableTemplate(lit("<table>%1</table>"));
    static const QString kHtmlTableRowTemplate(lit("<tr>%1</tr>"));
};

QnUserSettingsDialog::QnUserSettingsDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::UserSettingsDialog()),
    m_model(new QnUserSettingsModel(this)),
    m_user(),
    m_profilePage(new QnUserProfileWidget(m_model, this)),
    m_settingsPage(new QnUserSettingsWidget(m_model, this)),
    m_permissionsPage(new QnPermissionsWidget(m_model, this)),
    m_camerasPage(new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::CamerasFilter, this)),
    m_layoutsPage(new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::LayoutsFilter, this)),
    m_editGroupsButton(new QPushButton(tr("Edit Roles..."), this))
{
    ui->setupUi(this);

    addPage(ProfilePage, m_profilePage, tr("Profile"));
    addPage(SettingsPage, m_settingsPage, tr("User Information"));
    addPage(PermissionsPage, m_permissionsPage, tr("Permissions"));
    addPage(CamerasPage, m_camerasPage, tr("Media Resources"));
    addPage(LayoutsPage, m_layoutsPage, tr("Layouts"));

    connect(m_settingsPage,     &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnUserSettingsDialog::permissionsChanged);
    connect(m_permissionsPage,  &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnUserSettingsDialog::permissionsChanged);
    connect(m_camerasPage,      &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnUserSettingsDialog::permissionsChanged);
    connect(m_layoutsPage,      &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnUserSettingsDialog::permissionsChanged);

    auto selectionWatcher = new QnWorkbenchSelectionWatcher(this);
    connect(selectionWatcher, &QnWorkbenchSelectionWatcher::selectionChanged, this, [this](const QnResourceList &resources)
    {
        if (isHidden())
            return;

        /* Do not automatically switch if we are creating a new user. */
        if (m_user && m_user->flags().testFlag(Qn::local))
            return;

        auto users = resources.filtered<QnUserResource>();
        if (!users.isEmpty())
            setUser(users.first());
    });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr& resource)
    {
        if (resource != m_user)
        {
            //TODO: #vkutin #GDM check if permissions change is correctly handled through a chain of signals
            return;
        }
        setUser(QnUserResourcePtr());
        tryClose(true);
    });

    ui->buttonBox->addButton(m_editGroupsButton, QDialogButtonBox::HelpRole);
    connect(m_editGroupsButton, &QPushButton::clicked, this, [this]
    {
        QnUuid groupId = isPageVisible(ProfilePage) ? m_user->userGroup() : m_settingsPage->selectedUserGroup();
        menu()->trigger(QnActions::UserGroupsAction, QnActionParameters().withArgument(Qn::ResourceUidRole, groupId));
    });

    m_editGroupsButton->setVisible(false);

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);

    permissionsChanged();
}

QnUserSettingsDialog::~QnUserSettingsDialog()
{
}

QnUserResourcePtr QnUserSettingsDialog::user() const
{
    return m_user;
}

void QnUserSettingsDialog::permissionsChanged()
{
    updateControlsVisibility();

    auto descriptionHtml = [](QnResourceAccessFilter::Filter filter, bool all, const std::pair<int, int>& counts)
    {
        QString name = kCategoryNameByFilter[filter];
        if (all)
            return lit("<td colspan=2><b>%1 %2</b></td>").arg(tr("All")).arg(name);

        if (counts.second < 0)
            return lit("<td><b>%1&nbsp;</b></td><td>%2</td>").arg(counts.first).arg(name);

        return lit("<td><b>%1</b> / %2&nbsp;</td><td>%3</td>").arg(counts.first).arg(counts.second).arg(name);
    };

    auto descriptionById = [this, descriptionHtml](QnResourceAccessFilter::Filter filter, QnUuid id, Qn::GlobalPermissions permissions, bool currentUserIsAdmin)
    {
        auto allResources = qnResPool->getResources();
        auto accessibleResources = qnResourceAccessManager->accessibleResources(id);

        std::pair<int, int> counts(0, currentUserIsAdmin ? 0 : -1);

        if (filter == QnResourceAccessFilter::CamerasFilter &&
            permissions.testFlag(Qn::GlobalAccessAllCamerasPermission))
        {
            return descriptionHtml(filter, true, counts);
        }

        for (QnResourcePtr resource : allResources)
        {
            if (QnAccessibleResourcesWidget::resourcePassFilter(resource, context()->user(), filter))
            {
                if (currentUserIsAdmin)
                    ++counts.second;

                if (accessibleResources.contains(resource->getId()))
                    ++counts.first;
            }
        }

        return descriptionHtml(filter, false, counts);
    };

    if (isPageVisible(ProfilePage))
    {
        Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_user);
        QnUuid groupId = m_user->userGroup();

        QString permissionsText = accessController()->userRoleDescription(m_user);

        if (!groupId.isNull())
        {
            /* Handle custom user role: */
            Qn::GlobalPermissions groupPermissions = qnResourceAccessManager->userGroup(groupId).permissions;

            permissionsText += kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::CamerasFilter, groupId, groupPermissions, false)) +
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, groupId, groupPermissions, false)));
        }
        else if (!m_user->isOwner() && !permissions.testFlag(Qn::GlobalAdminPermission))
        {
            switch (permissions)
            {
                //TODO: #vkutin #GDM Refactor all this logic; introduce standard role descriptors to check against and fetch names from
                case Qn::GlobalAdvancedViewerPermissionSet:
                case Qn::GlobalViewerPermissionSet:
                case Qn::GlobalLiveViewerPermissionSet:
                    break;

                default:
                {
                    /* Handle custom permissions: */
                    QnUuid userId = m_user->getId();

                    permissionsText += kHtmlTableTemplate.arg(
                        kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::CamerasFilter, userId, permissions, false)) +
                        kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, userId, permissions, false)));
                }
            }
        }

        m_profilePage->updatePermissionsLabel(permissionsText);
    }
    else
    {
        Qn::GlobalPermissions permissions = m_settingsPage->selectedPermissions();
        QnUuid groupId = m_settingsPage->selectedUserGroup();

        QString permissionsText = accessController()->userRoleDescription(permissions, groupId);

        if (isPageVisible(PermissionsPage))
        {
            /* Handle custom permissions: */
            auto descriptionFromWidget = [descriptionHtml](const QnAccessibleResourcesWidget* widget)
            {
                return descriptionHtml(widget->filter(), widget->isAll(), widget->selected());
            };

            permissionsText += kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionFromWidget(m_camerasPage)) +
                kHtmlTableRowTemplate.arg(descriptionFromWidget(m_layoutsPage)));
        }
        else if (!groupId.isNull())
        {
            /* Handle custom user role: */
            Qn::GlobalPermissions groupPermissions = qnResourceAccessManager->userGroup(groupId).permissions;

            permissionsText += kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::CamerasFilter, groupId, groupPermissions, true)) +
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, groupId, groupPermissions, true)));
        }

        m_settingsPage->updatePermissionsLabel(permissionsText);
    }
}

void QnUserSettingsDialog::setUser(const QnUserResourcePtr &user)
{
    if(m_user == user)
        return;

    if (!tryClose(false))
        return;

    m_user = user;
    m_model->setUser(user);

    /* Hide Apply button if cannot apply changes. */
    bool applyButtonVisible = m_model->mode() == QnUserSettingsModel::OwnProfile || m_model->mode() == QnUserSettingsModel::OtherSettings;
    ui->buttonBox->button(QDialogButtonBox::Apply)->setVisible(applyButtonVisible);

    /** Hide Cancel button if we cannot edit user. */
    bool cancelButtonVisible = m_model->mode() != QnUserSettingsModel::OtherProfile && m_model->mode() != QnUserSettingsModel::Invalid;
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(cancelButtonVisible);

    ui->tabWidget->setTabBarAutoHide(m_model->mode() == QnUserSettingsModel::OwnProfile
        || m_model->mode() == QnUserSettingsModel::OtherProfile);

    loadDataToUi();
    permissionsChanged();
}

QDialogButtonBox::StandardButton QnUserSettingsDialog::showConfirmationDialog()
{
    NX_ASSERT(m_user, Q_FUNC_INFO, "User must exist here");

    if (m_model->mode() != QnUserSettingsModel::OwnProfile && m_model->mode() != QnUserSettingsModel::OtherSettings)
        return QDialogButtonBox::No;

    if (!canApplyChanges())
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
            : tr("User Settings - %1").arg(m_user->getName())
            );
        setHelpTopic(this, Qn::UserSettings_Help);
    }
}

bool QnUserSettingsDialog::hasChanges() const
{
    return base_type::hasChanges();

    //if (base_type::hasChanges())
    //    return true;

    ///* Permissions are checked separately */
    //if (isPageVisible(SettingsPage))
    //{
    //    if (!m_settingsPage->selectedUserGroup().isNull())
    //        return false;

    //    Qn::GlobalPermissions basePermissions = m_settingsPage->selectedPermissions();
    //    if (basePermissions != Qn::NoGlobalPermissions)
    //        /* Predefined group selected */
    //        return basePermissions != qnResourceAccessManager->globalPermissions(m_user))


    //}



}

void QnUserSettingsDialog::applyChanges()
{
    auto mode = m_model->mode();
    if (mode == QnUserSettingsModel::Invalid || mode == QnUserSettingsModel::OtherProfile)
        return;

    //TODO: #GDM #access SafeMode what to rollback if current password changes cannot be saved?
    //TODO: #GDM #access SafeMode what to rollback if we were creating new user
    qnResourcesChangesManager->saveUser(m_user, [this, mode](const QnUserResourcePtr& user)
    {
        Q_UNUSED(user);
        for (const auto& page : allPages())
        {
            if (page.enabled && page.visible)
                page.widget->applyChanges();
        }
    });
    /* We may fill password field to change current user password. */
    m_user->setPassword(QString());

    if (m_model->mode() == QnUserSettingsModel::NewUser)
    {
        /* New User was created, clear dialog. */
        setUser(QnUserResourcePtr());
    }

    updateButtonBox();
}

void QnUserSettingsDialog::updateControlsVisibility()
{
    auto mode = m_model->mode();

    /* We are displaying profile for ourself and for users, that we cannot edit. */
    bool profilePageVisible = mode == QnUserSettingsModel::OwnProfile
        || mode == QnUserSettingsModel::OtherProfile;

    bool settingsPageVisible = mode == QnUserSettingsModel::NewUser
        || mode == QnUserSettingsModel::OtherSettings;

    bool customAccessRights = settingsPageVisible
        && m_settingsPage->selectedPermissions() == Qn::NoGlobalPermissions
        && m_settingsPage->selectedUserGroup().isNull();

    setPageVisible(ProfilePage,     profilePageVisible);

    setPageVisible(SettingsPage,    settingsPageVisible);
    setPageVisible(PermissionsPage, customAccessRights);
    setPageVisible(CamerasPage,     customAccessRights);
    setPageVisible(LayoutsPage,     customAccessRights);

    m_editGroupsButton->setVisible(settingsPageVisible);

    /* Buttons state takes into account pages visibility, so we must recalculate it. */
    updateButtonBox();
}

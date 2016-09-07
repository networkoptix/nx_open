#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

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
        { QnResourceAccessFilter::MediaFilter, QnUserSettingsDialog::tr("Media Resources") },
        { QnResourceAccessFilter::LayoutsFilter, QnUserSettingsDialog::tr("Shared Layouts") }
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
    m_camerasPage(new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::MediaFilter, this)),
    m_layoutsPage(new QnAccessibleResourcesWidget(m_model, QnResourceAccessFilter::LayoutsFilter, this)),
    m_editGroupsButton(new QPushButton(tr("Edit Roles..."), this))
{
    ui->setupUi(this);

    addPage(ProfilePage, m_profilePage, tr("Profile"));
    addPage(SettingsPage, m_settingsPage, tr("User Information"));
    addPage(PermissionsPage, m_permissionsPage, tr("Permissions"));
    addPage(CamerasPage, m_camerasPage, tr("Media Resources"));
    addPage(LayoutsPage, m_layoutsPage, tr("Layouts"));

    connect(qnResourceAccessManager, &QnResourceAccessManager::permissionsInvalidated, this,
        [this](const QSet<QnUuid>& resourceIds)
        {
            if (m_user && resourceIds.contains(m_user->getId()))
                permissionsChanged();
        });

    connect(m_settingsPage,     &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::permissionsChanged);
    connect(m_permissionsPage,  &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::permissionsChanged);
    connect(m_camerasPage,      &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::permissionsChanged);
    connect(m_layoutsPage,      &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::permissionsChanged);

    connect(m_settingsPage,     &QnUserSettingsWidget::userTypeChanged,          this,
        [this](bool isCloud)
        {
            /* Kinda hack to change user type: we have to recreate user resource: */
            QnUserResourcePtr newUser(new QnUserResource(isCloud ? QnUserType::Cloud : QnUserType::Local));
            newUser->setFlags(m_user->flags());
            newUser->setId(m_user->getId());
            newUser->setRawPermissions(m_user->getRawPermissions());
            m_user = newUser;
            m_model->setUser(m_user);

    #if false //TODO: #common Enable this if we want to change OK button caption when creating a cloud user
            buttonBox()->button(QDialogButtonBox::Ok)->setText(
                isCloud ? tr("Send Invite") : QCoreApplication::translate("QPlatformTheme", "OK")); // As in Qt
    #endif
        });

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
    connect(m_editGroupsButton, &QPushButton::clicked, this,
        [this]
        {
            QnUuid groupId = isPageVisible(ProfilePage)
                ? m_user->userGroup()
                : m_settingsPage->selectedUserGroup();
            menu()->trigger(QnActions::UserRolesAction,
                QnActionParameters().withArgument(Qn::UuidRole, groupId));
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

        if (filter == QnResourceAccessFilter::MediaFilter &&
            permissions.testFlag(Qn::GlobalAccessAllMediaPermission))
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
        Qn::UserRole roleType = m_user->role();
        QString permissionsText = qnResourceAccessManager->userRoleDescription(roleType);

        if (roleType == Qn::UserRole::CustomUserGroup)
        {
            /* Handle custom user role: */
            QnUuid groupId = m_user->userGroup();
            Qn::GlobalPermissions groupPermissions = qnResourceAccessManager->userGroup(groupId).permissions;

            permissionsText += kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::MediaFilter, groupId, groupPermissions, false)) +
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, groupId, groupPermissions, false)));
        }
        else if (roleType == Qn::UserRole::CustomPermissions)
        {
            QnUuid userId = m_user->getId();
            Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_user);

            permissionsText += kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::MediaFilter, userId, permissions, false)) +
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, userId, permissions, false)));
        }

        m_profilePage->updatePermissionsLabel(permissionsText);
    }
    else
    {
        Qn::UserRole roleType = m_settingsPage->selectedRole();
        QString permissionsText = qnResourceAccessManager->userRoleDescription(roleType);

        if (roleType == Qn::UserRole::CustomUserGroup)
        {
            /* Handle custom user role: */
            QnUuid groupId = m_settingsPage->selectedUserGroup();
            Qn::GlobalPermissions groupPermissions = qnResourceAccessManager->userGroup(groupId).permissions;

            permissionsText += kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::MediaFilter, groupId, groupPermissions, true)) +
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, groupId, groupPermissions, true)));
        }
        else if (roleType == Qn::UserRole::CustomPermissions)
        {
            /* Handle custom permissions: */
            auto descriptionFromWidget = [descriptionHtml](const QnAccessibleResourcesWidget* widget)
            {
                return descriptionHtml(widget->filter(), widget->isAll(), widget->selected());
            };

            permissionsText += kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionFromWidget(m_camerasPage)) +
                kHtmlTableRowTemplate.arg(descriptionFromWidget(m_layoutsPage)));

            m_model->setAccessibleLayoutsPreview(m_layoutsPage->checkedResources());

            m_layoutsPage->indirectAccessChanged();
            m_camerasPage->indirectAccessChanged();
        }

        m_settingsPage->updatePermissionsLabel(permissionsText);
    }
}

void QnUserSettingsDialog::setUser(const QnUserResourcePtr &user)
{
    if (m_user == user)
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
            : tr("User Settings - %1").arg(m_user->getName()));

        setHelpTopic(this, Qn::UserSettings_Help);
    }
}

void QnUserSettingsDialog::applyChanges()
{
    auto mode = m_model->mode();
    if (mode == QnUserSettingsModel::Invalid || mode == QnUserSettingsModel::OtherProfile)
        return;

    if (m_user->getId().isNull())
        m_user->fillId();

    //TODO: #GDM #access SafeMode what to rollback if current password changes cannot be saved?
    //TODO: #GDM #access SafeMode what to rollback if we were creating new user
    qnResourcesChangesManager->saveUser(m_user,
        [this, mode](const QnUserResourcePtr& /*user*/)
        {
            //here accessible resources will also be filled
            applyChangesInternal();
        });

    auto accessibleResources = m_model->accessibleResources();

    QnLayoutResourceList layoutsToShare = qnResPool->getResources(accessibleResources)
        .filtered<QnLayoutResource>(
            [](const QnLayoutResourcePtr& layout)
    {
        return !layout->isFile() && !layout->isShared();
    });

    for (const auto& layout : layoutsToShare)
    {
        menu()->trigger(QnActions::ShareLayoutAction,
            QnActionParameters(layout).withArgument(Qn::UserResourceRole, m_user));
    }

    qnResourcesChangesManager->saveAccessibleResources(m_user, accessibleResources);

    /* We may fill password field to change current user password. */
    m_user->setPassword(QString());

    /* New User was created, clear dialog. */
    if (m_model->mode() == QnUserSettingsModel::NewUser)
        setUser(QnUserResourcePtr());

    updateButtonBox();
    loadDataToUi();
}

void QnUserSettingsDialog::showEvent(QShowEvent* event)
{
    loadDataToUi();
    base_type::showEvent(event);
}

void QnUserSettingsDialog::applyChangesInternal()
{
    base_type::applyChanges();
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
        && m_settingsPage->selectedRole() == Qn::UserRole::CustomPermissions;

    setPageVisible(ProfilePage,     profilePageVisible);

    setPageVisible(SettingsPage,    settingsPageVisible);
    setPageVisible(PermissionsPage, customAccessRights);
    setPageVisible(CamerasPage,     customAccessRights);
    setPageVisible(LayoutsPage,     customAccessRights);

    m_editGroupsButton->setVisible(settingsPageVisible);

    /* Buttons state takes into account pages visibility, so we must recalculate it. */
    updateButtonBox();
}

#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
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
        { QnResourceAccessFilter::MediaFilter, QnUserSettingsDialog::tr("Cameras & Resources") },
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
    m_editRolesButton(new QPushButton(tr("Edit Roles..."), this))
{
    ui->setupUi(this);

    addPage(ProfilePage, m_profilePage, tr("Profile"));
    addPage(SettingsPage, m_settingsPage, tr("User Information"));
    addPage(PermissionsPage, m_permissionsPage, tr("Permissions"));
    addPage(CamerasPage, m_camerasPage, tr("Cameras && Resources"));
    addPage(LayoutsPage, m_layoutsPage, tr("Layouts"));

    connect(qnResourceAccessManager, &QnResourceAccessManager::permissionsChanged, this,
        [this](const QnResourceAccessSubject& subject)
        {
            if (m_user && subject.user() == m_user)
                updatePermissions();
        });

    connect(m_settingsPage,     &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::updatePermissions);
    connect(m_permissionsPage,  &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::updatePermissions);
    connect(m_camerasPage,      &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::updatePermissions);
    connect(m_layoutsPage,      &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &QnUserSettingsDialog::updatePermissions);

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

    ui->buttonBox->addButton(m_editRolesButton, QDialogButtonBox::HelpRole);
    connect(m_editRolesButton, &QPushButton::clicked, this,
        [this]
        {
            QnUuid roleId = isPageVisible(ProfilePage)
                ? m_user->userRoleId()
                : m_settingsPage->selectedUserRoleId();
            menu()->trigger(QnActions::UserRolesAction,
                QnActionParameters().withArgument(Qn::UuidRole, roleId));
        });

    m_editRolesButton->setVisible(false);

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);

    updatePermissions();
}

QnUserSettingsDialog::~QnUserSettingsDialog()
{
}

QnUserResourcePtr QnUserSettingsDialog::user() const
{
    return m_user;
}

void QnUserSettingsDialog::updatePermissions()
{
    updateControlsVisibility();

    auto descriptionHtml =
        [](QnResourceAccessFilter::Filter filter, bool all, const std::pair<int, int>& counts)
        {
            static const QString kHtmlRowTemplate1 =
                lit("<td><b>%1</b></td><td width=16/><td>%2</td>");
            static const QString kHtmlRowTemplate2 =
                lit("<td><b>%1</b> / %2</td><td width=16/><td>%3</td>");

            QString name = kCategoryNameByFilter[filter];
            if (all)
            {
                //: This will be a part of "All Cameras & Resources" or "All Shared Layouts"
                return kHtmlRowTemplate1.arg(tr("All")).arg(name);
            }

            if (counts.second < 0)
                return kHtmlRowTemplate1.arg(counts.first).arg(name);

            return kHtmlRowTemplate2.arg(counts.first).arg(counts.second).arg(name);
        };

    auto descriptionById =
        [this, descriptionHtml](QnResourceAccessFilter::Filter filter,
            QnResourceAccessSubject subject, bool currentUserIsAdmin)
        {
            auto allResources = qnResPool->getResources();
            auto accessibleResources = qnSharedResourcesManager->sharedResources(subject);
            auto permissions = qnResourceAccessManager->globalPermissions(subject);

            std::pair<int, int> counts(0, currentUserIsAdmin ? 0 : -1);

            if (filter == QnResourceAccessFilter::MediaFilter &&
                permissions.testFlag(Qn::GlobalAccessAllMediaPermission))
            {
                return descriptionHtml(filter, true, counts);
            }

            auto currentUser = context()->user();
            for (QnResourcePtr resource : allResources)
            {
                if (QnAccessibleResourcesWidget::resourcePassFilter(resource, currentUser, filter))
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
        Qn::UserRole role = m_user->userRole();
        QString permissionsText;

        if (role == Qn::UserRole::CustomUserRole || role == Qn::UserRole::CustomPermissions)
        {
            QnResourceAccessSubject subject(m_user);
            permissionsText = kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::MediaFilter, subject, false)) +
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, subject, false)));
        }
        else
        {
            permissionsText = QnUserRolesManager::userRoleDescription(role);
        }

        m_profilePage->updatePermissionsLabel(permissionsText);
    }
    else
    {
        Qn::UserRole roleType = m_settingsPage->selectedRole();
        QString permissionsText;

        if (roleType == Qn::UserRole::CustomUserRole)
        {
            /* Handle custom user role: */
            QnUuid roleId = m_settingsPage->selectedUserRoleId();
            QnResourceAccessSubject subject(qnUserRolesManager->userRole(roleId));

            permissionsText = kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::MediaFilter, subject, true)) +
                kHtmlTableRowTemplate.arg(descriptionById(QnResourceAccessFilter::LayoutsFilter, subject, true)));
        }
        else if (roleType == Qn::UserRole::CustomPermissions)
        {
            /* Handle custom permissions: */
            auto descriptionFromWidget = [descriptionHtml](const QnAccessibleResourcesWidget* widget)
            {
                return descriptionHtml(widget->filter(), widget->isAll(), widget->selected());
            };

            permissionsText = kHtmlTableTemplate.arg(
                kHtmlTableRowTemplate.arg(descriptionFromWidget(m_camerasPage)) +
                kHtmlTableRowTemplate.arg(descriptionFromWidget(m_layoutsPage)));
        }
        else
        {
            permissionsText = QnUserRolesManager::userRoleDescription(roleType);
        }

        m_settingsPage->updatePermissionsLabel(permissionsText);
    }

    m_model->updatePermissions();
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

    forcedUpdate();
}

void QnUserSettingsDialog::forcedUpdate()
{
    base_type::forcedUpdate();
    updatePermissions();
    updateButtonBox();
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

    //TODO: #GDM #access SafeMode what to rollback if current password changes cannot be saved?
    //TODO: #GDM #access SafeMode what to rollback if we were creating new user
    qnResourcesChangesManager->saveUser(m_user,
        [this, mode](const QnUserResourcePtr& /*user*/)
        {
            //here accessible resources will also be filled to model
            applyChangesInternal();

            if (m_user->getId().isNull())
                m_user->fillId();
        });

    if (m_user->userRole() == Qn::UserRole::CustomPermissions)
    {
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
    }

    /* We may fill password field to change current user password. */
    m_user->setPassword(QString());

    /* New User was created, clear dialog. */
    if (m_model->mode() == QnUserSettingsModel::NewUser)
        setUser(QnUserResourcePtr());

    forcedUpdate();
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

    m_editRolesButton->setVisible(settingsPageVisible);

    /* Buttons state takes into account pages visibility, so we must recalculate it. */
    updateButtonBox();
}

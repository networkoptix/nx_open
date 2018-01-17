#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <QtWidgets/QPushButton>

#include <client_core/connection_context_aware.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/models/resource_properties/user_settings_model.h>
#include <ui/widgets/properties/user_profile_widget.h>
#include <ui/widgets/properties/user_settings_widget.h>
#include <ui/widgets/properties/accessible_resources_widget.h>
#include <ui/widgets/properties/permissions_widget.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <ui/workbench/watchers/workbench_selection_watcher.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/html.h>

using namespace nx::client::desktop::ui;

namespace {

// TODO: #ak #move to cdb api section
static const QString cloudAuthInfoPropertyName(lit("cloudUserAuthenticationInfo"));

class PermissionsInfoTable: public QnConnectionContextAware
{
    Q_DECLARE_TR_FUNCTIONS(PermissionsInfoTable)
public:
    PermissionsInfoTable(const QnUserResourcePtr& currentUser):
        m_currentUser(currentUser)
    {}

    void addPermissionsRow(const QnResourceAccessSubject& subject)
    {
        auto permissions = resourceAccessManager()->globalPermissions(subject);
        addRow(makePermissionsRow(permissions));
    }

    void addPermissionsRow(QnPermissionsWidget* widget)
    {
        addRow(makePermissionsRow(widget->selectedPermissions()));
    }

    void addResourceAccessRow(QnResourceAccessFilter::Filter filter,
            const QnResourceAccessSubject& subject, bool currentUserIsAdmin)
    {
        auto allResources = resourcePool()->getResources();
        auto accessibleResources = sharedResourcesManager()->sharedResources(subject);
        auto permissions = resourceAccessManager()->globalPermissions(subject);

        int count = 0;
        int total = currentUserIsAdmin ? 0 : -1;

        if (filter == QnResourceAccessFilter::MediaFilter &&
            permissions.testFlag(Qn::GlobalAccessAllMediaPermission))
        {
            addRow(makeResourceAccessRow(filter, true, count, total));
            return;
        }

        for (const auto& resource: allResources)
        {
            if (QnAccessibleResourcesWidget::resourcePassFilter(resource, m_currentUser, filter))
            {
                if (currentUserIsAdmin)
                    ++total;

                if (accessibleResources.contains(resource->getId()))
                    ++count;
            }
        }

        addRow(makeResourceAccessRow(filter, false, count, total));
    };


    void addResourceAccessRow(const QnAccessibleResourcesWidget* widget)
    {
        const auto countAndTotal = widget->selected();
        addRow(makeResourceAccessRow(widget->filter(), widget->isAll(), countAndTotal.first,
            countAndTotal.second));
    }

    QString makeTable() const
    {
        QString result;
        {
            QnHtmlTag tableTag("table", result, QnHtmlTag::NoBreaks);
            for (const auto& row: m_rows)
            {
                QnHtmlTag rowTag("tr", result, QnHtmlTag::NoBreaks);
                result.append(row);
            }
        }
        return result;
    }

private:
    void addRow(const QString& row)
    {
        m_rows.push_back(row);
    }

    QString categoryName(QnResourceAccessFilter::Filter value) const
    {
        switch (value)
        {
            case QnResourceAccessFilter::MediaFilter:
                return tr("Cameras & Resources");

            case QnResourceAccessFilter::LayoutsFilter:
                return tr("Shared Layouts");
            default:
                break;
        }

        return QString();
    }

    QString makeGenericCountRow(const QString& name, const QString& count) const
    {
        static const QString kHtmlRowTemplate =
            lit("<td><b>%1</b></td><td width=16/><td>%2</td>");
        return kHtmlRowTemplate.arg(count).arg(name);
    }

    QString makeGenericCountRow(const QString& name, int count) const
    {
        return makeGenericCountRow(name, QString::number(count));
    }

    QString makeGenericCountOfTotalRow(const QString& name, int count, int total) const
    {
        static const QString kHtmlRowTemplate =
            lit("<td><b>%1</b> / %2</td><td width=16/><td>%3</td>");
        return kHtmlRowTemplate.arg(count).arg(total).arg(name);
    }

    QString makeResourceAccessRow(QnResourceAccessFilter::Filter filter, bool all,
        int count, int total) const
    {
        QString name = categoryName(filter);
        if (all) //: This will be a part of "All Cameras & Resources" or "All Shared Layouts"
            return makeGenericCountRow(name, tr("All")); // TODO: #GDM #tr make sure comment is handled

        if (filter == QnResourceAccessFilter::LayoutsFilter || total < 0)
            return makeGenericCountRow(name, count);

        return makeGenericCountOfTotalRow(name, count, total);
    };

    QString makePermissionsRow(Qn::GlobalPermissions rawPermissions) const
    {
        int count = 0;
        int total = 0;

        auto checkFlag = [&count, &total, rawPermissions](Qn::GlobalPermission flag)
            {
                ++total;
                if (rawPermissions.testFlag(flag))
                    ++count;
            };

        // TODO: #GDM think where to store flags set to avoid duplication
        checkFlag(Qn::GlobalEditCamerasPermission);
        checkFlag(Qn::GlobalControlVideoWallPermission);
        checkFlag(Qn::GlobalViewLogsPermission);
        checkFlag(Qn::GlobalViewArchivePermission);
        checkFlag(Qn::GlobalExportPermission);
        checkFlag(Qn::GlobalViewBookmarksPermission);
        checkFlag(Qn::GlobalManageBookmarksPermission);
        checkFlag(Qn::GlobalUserInputPermission);

        return makeGenericCountOfTotalRow(tr("Permissions"), count, total);
    }

private:
    QnUserResourcePtr m_currentUser;
    QStringList m_rows;
};

}; // namespace

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
    m_userEnabledButton(new QPushButton(tr("Enabled"), this))
{
    ui->setupUi(this);

    addPage(ProfilePage, m_profilePage, tr("User Information"));
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
        this, &QnUserSettingsDialog::updatePermissions);
    connect(m_permissionsPage, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &QnUserSettingsDialog::updatePermissions);
    connect(m_camerasPage, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &QnUserSettingsDialog::updatePermissions);
    connect(m_layoutsPage, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &QnUserSettingsDialog::updatePermissions);

    connect(m_settingsPage, &QnUserSettingsWidget::userTypeChanged, this,
        [this](bool isCloud)
        {
            /* Kinda hack to change user type: we have to recreate user resource: */
            QnUserResourcePtr newUser(new QnUserResource(isCloud ? QnUserType::Cloud : QnUserType::Local));
            newUser->setFlags(m_user->flags());
            newUser->setId(m_user->getId());
            newUser->setRawPermissions(m_user->getRawPermissions());
            m_user = newUser;
            m_model->setUser(m_user);

    #if false // TODO: #common Enable this if we want to change OK button caption when creating a cloud user
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
        if (m_model->mode() == QnUserSettingsModel::Mode::NewUser)
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

    ui->buttonBox->addButton(m_userEnabledButton, QDialogButtonBox::HelpRole);
    connect(m_userEnabledButton, &QPushButton::clicked, this, &QnUserSettingsDialog::updateButtonBox);

    m_userEnabledButton->setFlat(true);
    m_userEnabledButton->setCheckable(true);
    m_userEnabledButton->setVisible(false);
    setHelpTopic(m_userEnabledButton, Qn::UserSettings_DisableUser_Help);

    ui->alertBar->setReservedSpace(false);

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

    const auto currentUser = context()->user();
    if (!currentUser)
        return;

    PermissionsInfoTable helper(currentUser);
    if (isPageVisible(ProfilePage))
    {
        Qn::UserRole role = m_user->userRole();
        QString permissionsText;

        if (role == Qn::UserRole::CustomUserRole || role == Qn::UserRole::CustomPermissions)
        {
            QnResourceAccessSubject subject(m_user);
            helper.addPermissionsRow(subject);
            helper.addResourceAccessRow(QnResourceAccessFilter::MediaFilter, subject, false);
            helper.addResourceAccessRow(QnResourceAccessFilter::LayoutsFilter, subject, false);
            permissionsText = helper.makeTable();
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
            QnResourceAccessSubject subject(userRolesManager()->userRole(roleId));

            helper.addPermissionsRow(subject);
            helper.addResourceAccessRow(QnResourceAccessFilter::MediaFilter, subject, true);
            helper.addResourceAccessRow(QnResourceAccessFilter::LayoutsFilter, subject, true);
            permissionsText = helper.makeTable();
        }
        else if (roleType == Qn::UserRole::CustomPermissions)
        {
            helper.addPermissionsRow(m_permissionsPage);
            helper.addResourceAccessRow(m_camerasPage);
            helper.addResourceAccessRow(m_layoutsPage);
            permissionsText = helper.makeTable();
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

    /* Hide Apply button if cannot apply changes. */
    bool applyButtonVisible = m_model->mode() == QnUserSettingsModel::OwnProfile
                           || m_model->mode() == QnUserSettingsModel::OtherSettings;
    ui->buttonBox->button(QDialogButtonBox::Apply)->setVisible(applyButtonVisible);

    /** Hide Cancel button if we cannot edit user. */
    bool cancelButtonVisible = m_model->mode() != QnUserSettingsModel::OtherProfile
                            && m_model->mode() != QnUserSettingsModel::Invalid;
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(cancelButtonVisible);

    forcedUpdate();
}

void QnUserSettingsDialog::loadDataToUi()
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

void QnUserSettingsDialog::forcedUpdate()
{
    Qn::updateGuarded(this, [this]() { base_type::forcedUpdate(); });
    updatePermissions();
    updateButtonBox();
}

QDialogButtonBox::StandardButton QnUserSettingsDialog::showConfirmationDialog()
{
    NX_ASSERT(m_user, Q_FUNC_INFO, "User must exist here");

    if (m_model->mode() != QnUserSettingsModel::OwnProfile && m_model->mode() != QnUserSettingsModel::OtherSettings)
        return QDialogButtonBox::Cancel;

    if (!canApplyChanges())
        return QDialogButtonBox::Cancel;

    return QnMessageBox::question(this,
        tr("Apply changes before switching to another user?"), QString(),
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply);
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

    // TODO: #GDM #access SafeMode what to rollback if current password changes cannot be saved?

    auto applyChangesFunction =
        [this](const QnUserResourcePtr& /*user*/)
        {
            //here accessible resources will also be filled to model
            applyChangesInternal();
            if (m_user->getId().isNull())
                m_user->fillId();
        };

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

    /* We may fill password field to change current user password. */
    m_user->resetPassword();

    /* New User was created, clear dialog. */
    if (m_model->mode() == QnUserSettingsModel::NewUser)
        setUser(QnUserResourcePtr());

    forcedUpdate();
}

void QnUserSettingsDialog::applyChangesInternal()
{
    base_type::applyChanges();

    if (accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission))
        m_user->setEnabled(m_userEnabledButton->isChecked());
}

bool QnUserSettingsDialog::hasChanges() const
{
    if (base_type::hasChanges())
        return true;

    if (m_user && accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission))
        return (m_user->isEnabled() != m_userEnabledButton->isChecked());

    return false;
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

    m_userEnabledButton->setVisible(settingsPageVisible && m_user
        && accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission));

    /* Buttons state takes into account pages visibility, so we must recalculate it. */
    updateButtonBox();
}

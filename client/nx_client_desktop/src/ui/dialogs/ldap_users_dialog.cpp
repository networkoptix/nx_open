#include "ldap_users_dialog.h"
#include "ui_ldap_users_dialog.h"

#include <QtCore/QTimer>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <ui/style/custom_style.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/ldap_user_list_model.h>
#include <ui/models/user_roles_model.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/views/checkboxed_header_view.h>

#include <utils/common/ldap.h>
#include <common/common_module.h>

namespace {

// TODO: #GDM move timeout constant to more common module
static const int testLdapTimeoutMSec = 30 * 1000; //ec2::RESPONSE_WAIT_TIMEOUT_MS;

static const int kUpdateFilterDelayMs = 200;

}

QnLdapUsersDialog::QnLdapUsersDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::LdapUsersDialog),
    m_timeoutTimer(new QTimer(this)),
    m_rolesModel(new QnUserRolesModel(this,
        QnUserRolesModel::StandardRoleFlag
      | QnUserRolesModel::UserRoleFlag
      | QnUserRolesModel::AssignableFlag))
{
    ui->setupUi(this);

    setWarningStyle(ui->errorLabel);
    ui->errorLabel->hide();

    setHelpTopic(ui->selectRoleLabel, ui->userRoleComboBox, Qn::UserSettings_UserRoles_Help);
    ui->userRoleComboBox->setModel(m_rolesModel);
    ui->userRoleComboBox->setCurrentIndex(m_rolesModel->rowForRole(Qn::UserRole::LiveViewer)); // sensible default

    const QnLdapSettings &settings = qnGlobalSettings->ldapSettings();

    if (!settings.isValid()) {
        stopTesting(tr("The provided settings are not valid.") + lit("\n") + tr("Could not perform a test."));
        return;
    }

    QnMediaServerConnectionPtr serverConnection;
    const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
    for (const QnMediaServerResourcePtr server: onlineServers)
    {
        if (!(server->getServerFlags() & Qn::SF_HasPublicIP))
            continue;

        serverConnection = server->apiConnection();
        break;
    }

    if (!serverConnection)
    {
        QnMediaServerResourcePtr server = commonModule()->currentServer();

        NX_ASSERT(server);
        if (!server)
        {
            stopTesting(tr("Could not load users."));
            return;
        }

        serverConnection = server->apiConnection();
    }

    m_importButton = new QPushButton(this);
    m_importButton->setText(tr("Import Selected"));
    setAccentStyle(m_importButton);
    ui->buttonBox->addButton(m_importButton, QnDialogButtonBox::HelpRole);
    m_importButton->setVisible(false);

    ui->buttonBox->showProgress();
    m_timeoutTimer->setInterval(testLdapTimeoutMSec);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]{
        stopTesting(tr("Timed Out"));
    });
    m_timeoutTimer->start();

    serverConnection->testLdapSettingsAsync(settings,
        this,
        SLOT(at_testLdapSettingsFinished(int, const QnLdapUsers &,int, const QString &)));

    setHelpTopic(this, Qn::UserSettings_LdapFetch_Help);
}

QnLdapUsersDialog::~QnLdapUsersDialog() {}

void QnLdapUsersDialog::at_testLdapSettingsFinished(int status,
    const QnLdapUsers& users,
    int /*handle*/,
    const QString& errorString)
{
    if (!m_loading)
        return;

    if (status != 0 || !errorString.isEmpty())
    {
        QString result = tr("Error while loading users.");
        if (!errorString.isEmpty())
            result += lit(" (%1)").arg(errorString);

        stopTesting(result);
        return;
    }

    updateExistingUsers(users);

    QnLdapUsers filteredUsers = filterExistingUsers(users);
    if (filteredUsers.isEmpty())
    {
        stopTesting(tr("No new users found."));
        return;
    }

    m_loading = false;

    setupUsersTable(filteredUsers);

    ui->stackedWidget->setCurrentWidget(ui->usersPage);
    ui->buttonBox->hideProgress();

    m_importButton->setVisible(true);
    connect(m_importButton, &QPushButton::clicked, this,
        [this]
        {
            importUsers(visibleSelectedUsers());
            accept();
        });
}

void QnLdapUsersDialog::stopTesting(const QString &text /* = QString()*/) {
    if (!m_loading)
        return;

    m_loading = false;

    m_timeoutTimer->stop();
    ui->buttonBox->hideProgress();
    ui->loadingLabel->setVisible(false);
    ui->errorLabel->setText(text);
    ui->errorLabel->setVisible(true);
}

void QnLdapUsersDialog::updateExistingUsers(const QnLdapUsers &users)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto importedUsers = resourcePool()->getResources().filtered<QnUserResource>(
        [](const QnUserResourcePtr &user)
        {
            return user->isLdap();
        });

    QnUserResourceList modifiedUsers;

    for (const auto& ldapUser: users)
    {
        auto it = std::find_if(importedUsers.cbegin(), importedUsers.cend(),
            [ldapUser](const QnUserResourcePtr &user)
            {
                return QString::compare(ldapUser.login, user->getName(), Qt::CaseInsensitive) == 0;
            });

        if (it == importedUsers.cend())
            continue;

        QnUserResourcePtr existingUser = *it;
        if (existingUser->getEmail() == ldapUser.email)
            continue;

        existingUser->setEmail(ldapUser.email);
        modifiedUsers.push_back(existingUser);
    }

    qnResourcesChangesManager->saveUsers(modifiedUsers);
}

void QnLdapUsersDialog::importUsers(const QnLdapUsers &users)
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    // Safety check
    auto filteredUsers = filterExistingUsers(users);

    // Double semantic negation here to avoid checkbox name-content inconsistency.
    const bool enableUsers = !ui->disableUsersCheckBox->isChecked();
    const Qn::UserRole selectedRole = ui->userRoleComboBox->itemData(
        ui->userRoleComboBox->currentIndex(), Qn::UserRoleRole).value<Qn::UserRole>();
    const Qn::GlobalPermissions permissions = QnUserRolesManager::userRolePermissions(selectedRole);
    const QnUuid selectedUserRoleId = ui->userRoleComboBox->itemData(
        ui->userRoleComboBox->currentIndex(), Qn::UuidRole).value<QnUuid>();

    QnUserResourceList addedUsers;
    for (const auto& ldapUser: filteredUsers)
    {
        QnUserResourcePtr user(new QnUserResource(QnUserType::Ldap));
        user->setName(ldapUser.login);
        user->setEmail(ldapUser.email);
        user->fillId();
        user->setEnabled(enableUsers);
        if (selectedRole == Qn::UserRole::CustomUserRole)
            user->setUserRoleId(selectedUserRoleId);
        else
            user->setRawPermissions(permissions);
        addedUsers.push_back(user);
    }
    qnResourcesChangesManager->saveUsers(addedUsers);
}

QnLdapUsers QnLdapUsersDialog::filterExistingUsers(const QnLdapUsers &users) const
{
    QSet<QString> logins;
    for (const auto &user : resourcePool()->getResources<QnUserResource>())
        logins.insert(user->getName().toLower());

    QnLdapUsers result;
    for (const auto user : users)
    {
        if (logins.contains(user.login.toLower()))
            continue;
        result << user;
    }

    return result;
}

QnLdapUsers QnLdapUsersDialog::visibleUsers() const
{
    QnLdapUsers result;
    auto model = ui->usersTable->model();

    for (int row = 0; row < model->rowCount(); ++row)
    {
        QModelIndex index = model->index(row, QnLdapUserListModel::CheckBoxColumn);

        auto user = index.data(QnLdapUserListModel::LdapUserRole).value<QnLdapUser>();
        if (!user.login.isEmpty())
            result << user;
    }
    return result;
}

QnLdapUsers QnLdapUsersDialog::visibleSelectedUsers() const
{
    QnLdapUsers result;
    auto model = ui->usersTable->model();

    for (int row = 0; row < model->rowCount(); ++row)
    {
        QModelIndex index = model->index(row, QnLdapUserListModel::CheckBoxColumn);
        bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!checked)
            continue;
        auto user = index.data(QnLdapUserListModel::LdapUserRole).value<QnLdapUser>();
        if (!user.login.isEmpty())
            result << user;
    }
    return result;
}

void QnLdapUsersDialog::setupUsersTable(const QnLdapUsers& filteredUsers)
{
    auto scrollBar = new QnSnappedScrollBar(this);
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    auto usersModel = new QnLdapUserListModel(this);
    usersModel->setUsers(filteredUsers);

    auto sortModel = new QSortFilterProxyModel(this);
    auto header = new QnCheckBoxedHeaderView(QnLdapUserListModel::CheckBoxColumn, this);
    sortModel->setSourceModel(usersModel);

    connect(header, &QnCheckBoxedHeaderView::checkStateChanged, this,
        [this, usersModel](Qt::CheckState state)
        {
            QSet<QString> users;
            for (const auto& user: visibleUsers())
                users.insert(user.login);
            usersModel->setCheckState(state, users);
        });

    ui->usersTable->setModel(sortModel);
    ui->usersTable->setHeader(header);

    header->setVisible(true);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnLdapUserListModel::DnColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);

    ui->usersTable->sortByColumn(QnLdapUserListModel::FullNameColumn, Qt::AscendingOrder);

    auto updateSelection =
        [this, header, sortModel]
        {
            auto users = visibleSelectedUsers();
            Qt::CheckState selectionState = Qt::Unchecked;

            if (!users.isEmpty())
            {

                if (users.size() == sortModel->rowCount())
                    selectionState = Qt::Checked;
                else
                    selectionState = Qt::PartiallyChecked;
            }

            header->setCheckState(selectionState);
        };

    connect(ui->usersTable, &QTableView::clicked, this,
        [usersModel, updateSelection](const QModelIndex &index)
        {
            if (index.column() != QnLdapUserListModel::CheckBoxColumn)
                return;

            QString login = index.data(QnLdapUserListModel::LoginRole).toString();
            if (login.isEmpty())
                return;

            // Invert current state.
            const auto inverted = index.data(Qt::CheckStateRole).toInt() == Qt::Checked
                ? Qt::Unchecked
                : Qt::Checked;
            usersModel->setCheckState(inverted, login);
            updateSelection();

        });

    auto updateButton =
        [this, header]
        {
            m_importButton->setEnabled(header->checkState() != Qt::Unchecked);
        };

    // TODO: #GDM model should notify about its check state changes
    connect(header, &QnCheckBoxedHeaderView::checkStateChanged, this, updateButton);
    updateButton();

    sortModel->setDynamicSortFilter(true);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setFilterKeyColumn(-1);

    ui->filterLineEdit->setTextChangedSignalFilterMs(kUpdateFilterDelayMs);
    connect(ui->filterLineEdit, &QnSearchLineEdit::textChanged, this,
        [sortModel](const QString &text)
        {
            sortModel->setFilterWildcard(text);
        });
}

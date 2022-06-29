// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_users_dialog.h"
#include "ui_ldap_users_dialog.h"

#include <QtCore/QTimer>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/common/system_settings.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/ldap_user_list_model.h>
#include <ui/models/user_roles_model.h>
#include <utils/common/ldap.h>

using namespace nx;
using namespace nx::vms::client::desktop;

static const int kUpdateFilterDelayMs = 200;

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
    ui->userRoleComboBox->setCurrentIndex(m_rolesModel->rowForRole(Qn::UserRole::liveViewer)); // sensible default

    const QnLdapSettings &settings = globalSettings()->ldapSettings();

    if (!settings.isValid(/*checkPassword*/ false))
    {
        stopTesting(nx::format("%1\n%2").args(tr("The provided settings are not valid."),
            tr("Could not perform a test.")));
        return;
    }

    if (!NX_ASSERT(connection()))
    {
        stopTesting(tr("Could not load users."));
        return;
    }

    m_importButton = new QPushButton(this);
    m_importButton->setText(tr("Import Selected"));
    setAccentStyle(m_importButton);
    ui->buttonBox->addButton(m_importButton, QnDialogButtonBox::HelpRole);
    m_importButton->setVisible(false);

    ui->buttonBox->showProgress();
    m_timeoutTimer->setInterval(settings.searchTimeoutS * 1000);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]{
        stopTesting(tr("Timed Out"));
    });
    m_timeoutTimer->start();

    connectedServerApi()->testLdapSettingsAsync(settings,
        nx::utils::guarded(this,
        [this](bool success, int handle, const QnLdapUsers &users, const QString &errorString)
        {
            at_testLdapSettingsFinished(success, handle, users, errorString);
        }), thread());

    setHelpTopic(this, Qn::Ldap_Help);
}

QnLdapUsersDialog::~QnLdapUsersDialog() {}

void QnLdapUsersDialog::at_testLdapSettingsFinished(
    bool success, [[maybe_unused]]int handle,
    const QnLdapUsers &users, const QString &errorString)
{
    if (!m_loading)
        return;

    if (!success || !errorString.isEmpty())
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
            importUsers(selectedUsers());
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
    auto connection = messageBusConnection();
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
    auto connection = messageBusConnection();
    if (!connection)
        return;

    // Safety check
    auto filteredUsers = filterExistingUsers(users);

    // Double semantic negation here to avoid checkbox name-content inconsistency.
    const bool enableUsers = !ui->disableUsersCheckBox->isChecked();
    const Qn::UserRole selectedRole = ui->userRoleComboBox->itemData(
        ui->userRoleComboBox->currentIndex(), Qn::UserRoleRole).value<Qn::UserRole>();
    const QnUuid selectedUserRoleId = ui->userRoleComboBox->itemData(
        ui->userRoleComboBox->currentIndex(), Qn::UuidRole).value<QnUuid>();

    QnUserResourceList addedUsers;
    for (const auto& ldapUser: filteredUsers)
    {
        QnUserResourcePtr user(new QnUserResource(nx::vms::api::UserType::ldap));
        user->setName(ldapUser.login);
        user->setEmail(ldapUser.email);
        user->setFullName(ldapUser.fullName);
        user->fillIdUnsafe();
        user->setEnabled(enableUsers);
        user->setSingleUserRole(selectedUserRoleId);
        addedUsers.push_back(user);
    }

    const auto digestSupport = ui->allowDigestCheckBox->isChecked()
        ? QnUserResource::DigestSupport::enable
        : QnUserResource::DigestSupport::disable;

    qnResourcesChangesManager->saveUsers(addedUsers, digestSupport);
}

QnLdapUsers QnLdapUsersDialog::filterExistingUsers(const QnLdapUsers &users) const
{
    QSet<QString> logins;
    for (const auto& user: resourcePool()->getResources<QnUserResource>())
        logins.insert(user->getName().toLower());

    QnLdapUsers result;
    for (const auto& user: users)
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

QnLdapUsers QnLdapUsersDialog::selectedUsers(bool onlyVisible) const
{
    QnLdapUsers result;
    const auto model = onlyVisible
        ? static_cast<QAbstractItemModel*>(m_sortModel.get())
        : static_cast<QAbstractItemModel*>(m_usersModel.get());

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
    auto scrollBar = new SnappedScrollBar(this);
    ui->usersTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    m_usersModel = std::make_unique<QnLdapUserListModel>();
    m_usersModel->setUsers(filteredUsers);

    m_sortModel = std::make_unique<QSortFilterProxyModel>();
    m_sortModel->setSourceModel(m_usersModel.get());

    auto header = new CheckableHeaderView(QnLdapUserListModel::CheckBoxColumn, this);

    connect(header, &CheckableHeaderView::checkStateChanged, this,
        [this](Qt::CheckState state)
        {
            QSet<QString> users;
            for (const auto& user: visibleUsers())
                users.insert(user.login);
            m_usersModel->setCheckState(state, users);
        });

    ui->usersTable->setModel(m_sortModel.get());
    ui->usersTable->setHeader(header);

    header->setVisible(true);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnLdapUserListModel::DnColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);

    ui->usersTable->sortByColumn(QnLdapUserListModel::FullNameColumn, Qt::AscendingOrder);

    auto updateSelection =
        [this, header]
        {
            auto users = selectedUsers(/*onlyVisible*/ true);
            Qt::CheckState selectionState = Qt::Unchecked;

            if (!users.isEmpty())
            {

                if (users.size() == m_sortModel->rowCount())
                    selectionState = Qt::Checked;
                else
                    selectionState = Qt::PartiallyChecked;
            }

            header->setCheckState(selectionState);
        };

    connect(ui->usersTable, &QTableView::clicked, this,
        [this, updateSelection](const QModelIndex& index)
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
            m_usersModel->setCheckState(inverted, login);
            updateSelection();

        });

    auto updateButton =
        [this, header]
        {
            m_importButton->setEnabled(!selectedUsers().isEmpty());
        };

    // TODO: #sivanov Model should notify about its check state changes.
    connect(header, &CheckableHeaderView::checkStateChanged, this, updateButton);
    updateButton();

    m_sortModel->setDynamicSortFilter(true);
    m_sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortModel->setFilterKeyColumn(-1);

    ui->filterLineEdit->setTextChangedSignalFilterMs(kUpdateFilterDelayMs);
    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this,
        [this, updateSelection](const QString& text)
        {
            m_sortModel->setFilterWildcard(text);
            updateSelection();
        });
}

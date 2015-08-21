#include "ldap_users_dialog.h"
#include "ui_ldap_users_dialog.h"

#include <QtWidgets/QMessageBox>
#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <ui/style/warning_style.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/ldap_user_list_model.h>
#include <ui/widgets/views/checkboxed_header_view.h>

#include <utils/common/ldap.h>

namespace {
    //TODO: #GDM move timeout constant to more common module
    const int testLdapTimeoutMSec = 30 * 1000; //ec2::RESPONSE_WAIT_TIMEOUT_MS;
}

QnLdapUsersDialog::QnLdapUsersDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::LdapUsersDialog)
    , m_timeoutTimer(new QTimer(this))
    , m_loading(true)
{
    ui->setupUi(this);

    setWarningStyle(ui->errorLabel);
    ui->errorLabel->hide();

    const QnLdapSettings &settings = QnGlobalSettings::instance()->ldapSettings();

    if (!settings.isValid()) {
        stopTesting(tr("The provided settings are not valid.") + lit("\n") + tr("Could not perform a test."));
        return;
    }

    QnMediaServerConnectionPtr serverConnection;
    for (const QnMediaServerResourcePtr server: qnResPool->getAllServers()) {
        if (server->getStatus() != Qn::Online)
            continue;

        if (!(server->getServerFlags() & Qn::SF_HasPublicIP))
            continue;

        serverConnection = server->apiConnection();
        break;
    }

    if (!serverConnection) {
        stopTesting(tr("None of your servers is connected to the Internet.") + lit("\n") + tr("Could not load users."));
        return;
    }

    m_importButton = new QPushButton(this);
    m_importButton->setText(tr("Import users"));
    ui->buttonBox->addButton(m_importButton, QnDialogButtonBox::HelpRole);
    m_importButton->setVisible(false);

    ui->buttonBox->showProgress();
    m_timeoutTimer->setInterval(testLdapTimeoutMSec);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]{
        stopTesting(tr("Timed out"));
    });
    m_timeoutTimer->start();

    serverConnection->testLdapSettingsAsync(settings, this, SLOT(at_testLdapSettingsFinished(int, const QnLdapUsers &,int, const QString &)));

}

QnLdapUsersDialog::~QnLdapUsersDialog() {}

void QnLdapUsersDialog::at_testLdapSettingsFinished(int status, const QnLdapUsers &users, int handle, const QString &errorString) {
    if (!m_loading)
        return;

    if (status != 0 || !errorString.isEmpty()) {
        QString result;
        result = tr("Error while loading users.");
#if _DEBUG
        result += lit(" (%1)").arg(errorString);
#endif
        stopTesting(result);
        return;
    }

    updateExistingUsers(users);

    QnLdapUsers filteredUsers = filterExistingUsers(users);
    if (filteredUsers.isEmpty()) {
        stopTesting(tr("No new users found."));
        return;
    }

    m_loading = false;

    ui->stackedWidget->setCurrentWidget(ui->usersPage);
    ui->buttonBox->hideProgress();

    auto usersModel = new QnLdapUserListModel(this);
    usersModel->setUsers(filteredUsers);

    auto sortModel = new QSortFilterProxyModel(this);
    auto header = new QnCheckBoxedHeaderView(QnLdapUserListModel::CheckBoxColumn, this);
    sortModel->setSourceModel(usersModel);

    connect(header, &QnCheckBoxedHeaderView::checkStateChanged, this, [this, usersModel](Qt::CheckState state) {
        for (const auto &user: visibleUsers())
            usersModel->setCheckState(state, user.login);
    });

    ui->usersTable->setModel(sortModel);  
    ui->usersTable->setHorizontalHeader(header);

    header->setVisible(true);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnLdapUserListModel::DnColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);

    ui->usersTable->sortByColumn(QnLdapUserListModel::FullNameColumn, Qt::AscendingOrder);

    auto updateSelection = [this, header, sortModel] {
        auto users = visibleSelectedUsers();
        Qt::CheckState selectionState = Qt::Unchecked;

        if (!users.isEmpty()) {

            if (users.size() == sortModel->rowCount())
                selectionState = Qt::Checked;
            else
                selectionState = Qt::PartiallyChecked;
        }

        header->setCheckState(selectionState);
    };

    connect(ui->usersTable, &QTableView::clicked, this,  [usersModel, updateSelection] (const QModelIndex &index) {
        if (index.column() != QnLdapUserListModel::CheckBoxColumn) 
            return;

        QString login = index.data(QnLdapUserListModel::LoginRole).toString();
        if (login.isEmpty())
            return;

        /* Invert current state */
        usersModel->setCheckState(index.data(Qt::CheckStateRole).toBool() ? Qt::Unchecked : Qt::Checked, login);
        updateSelection();
        
    });

    m_importButton->setVisible(true);
    connect(m_importButton, &QPushButton::clicked, this, [this] {
        importUsers(visibleSelectedUsers());
        accept();
    });

    auto updateButton = [this, header] {
        m_importButton->setEnabled(header->checkState() != Qt::Unchecked);
    };

    //TODO: #GDM model should notify about its check state changes
    connect(header, &QnCheckBoxedHeaderView::checkStateChanged, this, updateButton);
    updateButton();

    sortModel->setDynamicSortFilter(true);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setFilterKeyColumn(-1);
    connect(ui->filterLineEdit,  &QLineEdit::textChanged, this, [sortModel, updateSelection](const QString &text) {
        sortModel->setFilterWildcard(text);
        updateSelection();
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

void QnLdapUsersDialog::updateExistingUsers(const QnLdapUsers &users) {
    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return;

    auto importedUsers = qnResPool->getResources<QnUserResource>().filtered([](const QnUserResourcePtr &user) {
        return user->isLdap();
    });  

    for (const QnLdapUser &ldapUser: users) {
        auto it = std::find_if(importedUsers.cbegin(), importedUsers.cend(), [ldapUser](const QnUserResourcePtr &user) {
            return QString::compare(ldapUser.login, user->getName(), Qt::CaseInsensitive) == 0;
        });

        if (it == importedUsers.cend())
            continue;

        QnUserResourcePtr existingUser = *it;
        if (existingUser->getEmail() == ldapUser.email)
            continue;

        existingUser->setEmail(ldapUser.email);

        connection->getUserManager()->save(existingUser, this,[] {} );
    }
}


void QnLdapUsersDialog::importUsers(const QnLdapUsers &users) {

    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return;

    /* Safety check */
    auto filteredUsers = filterExistingUsers(users);

    for (const QnLdapUser &ldapUser: filteredUsers) {
        QnUserResourcePtr user(new QnUserResource());
        user->setId(QnUuid::createUuid());
        user->setTypeByName(lit("User"));
        user->setPermissions(Qn::GlobalLiveViewerPermissions);
        user->setLdap(true);
        user->setEnabled(false);
        user->setName(ldapUser.login);
        user->setEmail(ldapUser.email);
        user->generateHash();
        connection->getUserManager()->save(user, this,[] {} );
    }
}

QnLdapUsers QnLdapUsersDialog::filterExistingUsers(const QnLdapUsers &users) const {
    QSet<QString> logins;
    for (const auto &user: qnResPool->getResources<QnUserResource>())
        logins.insert(user->getName().toLower());

    QnLdapUsers result;
    for (const auto user: users) {
        if (logins.contains(user.login.toLower()))
            continue;
        result << user;
    }

    return result;
}

QnLdapUsers QnLdapUsersDialog::visibleUsers() const {
    QnLdapUsers result;
    auto model = ui->usersTable->model();

    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex index = model->index(row, QnLdapUserListModel::CheckBoxColumn);

        auto user = index.data(QnLdapUserListModel::LdapUserRole).value<QnLdapUser>();
        if (!user.login.isEmpty())
            result << user;
    }
    return result;
}

QnLdapUsers QnLdapUsersDialog::visibleSelectedUsers() const {
    QnLdapUsers result;
    auto model = ui->usersTable->model();

    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex index = model->index(row, QnLdapUserListModel::CheckBoxColumn);
        bool checked = index.data(Qt::CheckStateRole).toBool();
        if (!checked)
            continue;
        auto user = index.data(QnLdapUserListModel::LdapUserRole).value<QnLdapUser>();
        if (!user.login.isEmpty())
            result << user;
    }
    return result;
}

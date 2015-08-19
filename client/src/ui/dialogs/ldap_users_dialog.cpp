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
    const int testLdapTimeoutMSec = 20 * 1000;
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
    m_importButton->setVisible(false);
    ui->buttonBox->addButton(m_importButton, QnDialogButtonBox::HelpRole);
    connect(m_importButton, &QPushButton::clicked, this, &QnLdapUsersDialog::importUsers);

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
    m_loading = false;

    if (status != 0 || !errorString.isEmpty()) {
        QString result;
        result = tr("Error while loading users.");
#if _DEBUG
        result += lit(" (%1)").arg(errorString);
#endif
        stopTesting(result);
        return;
    } 

    QnLdapUsers filteredUsers = filterExistingUsers(users);
    if (filteredUsers.isEmpty()) {
        stopTesting(tr("No new users found."));
        return;
    }

    ui->stackedWidget->setCurrentWidget(ui->usersPage);
    ui->buttonBox->hideProgress();
    m_importButton->setVisible(true);

    auto usersModel = new QnLdapUserListModel(this);
    usersModel->setUsers(filteredUsers);

    auto sortModel = new QSortFilterProxyModel(this);
    auto header = new QnCheckBoxedHeaderView(QnLdapUserListModel::CheckBoxColumn, this);
    sortModel->setSourceModel(usersModel);

    connect(header, &QnCheckBoxedHeaderView::checkStateChanged, this, [usersModel](Qt::CheckState state){
        usersModel->setCheckState(state);
    });

    ui->usersTable->setModel(sortModel);  
    ui->usersTable->setHorizontalHeader(header);

    header->setVisible(true);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnLdapUserListModel::DnColumn, QHeaderView::Stretch);
    header->setSectionsClickable(true);

    ui->usersTable->sortByColumn(QnLdapUserListModel::FullNameColumn, Qt::AscendingOrder);

    connect(ui->usersTable, &QTableView::clicked, this,  [usersModel, header] (const QModelIndex &index) {
        if (index.column() != QnLdapUserListModel::CheckBoxColumn) 
            return;

        QnLdapUser user = usersModel->getUser(index);
        if (user.login.isEmpty())
            return;

        /* Invert current state */
        usersModel->setCheckState(index.data(Qt::CheckStateRole).toBool() ? Qt::Unchecked : Qt::Checked, user.login);
        header->setCheckState(usersModel->checkState());
    });
}

void QnLdapUsersDialog::stopTesting(const QString &text /*= QString()*/) {
    if (!m_loading)
        return;

    m_loading = false;

    m_timeoutTimer->stop();
    ui->buttonBox->hideProgress();
    ui->loadingLabel->setVisible(false);
    ui->errorLabel->setText(text);
    ui->errorLabel->setVisible(true);
}

void QnLdapUsersDialog::importUsers() {

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

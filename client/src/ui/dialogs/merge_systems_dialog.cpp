#include "merge_systems_dialog.h"
#include "ui_merge_systems_dialog.h"

#include <QtCore/QUrl>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QMessageBox>

#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "ui/common/ui_resource_name.h"
#include "ui/style/warning_style.h"
#include "utils/merge_systems_tool.h"
#include "utils/common/util.h"

QnMergeSystemsDialog::QnMergeSystemsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnMergeSystemsDialog),
    m_mergeTool(new QnMergeSystemsTool(this))
{
    ui->setupUi(this);
    ui->urlComboBox->lineEdit()->setPlaceholderText(tr("http(s)://host:port"));
    m_mergeButton = ui->buttonBox->addButton(QString(), QDialogButtonBox::ActionRole);
    setWarningStyle(ui->errorLabel);
    m_mergeButton->hide();
    ui->buttonBox->button(QDialogButtonBox::Close)->hide();

    QButtonGroup *buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->currentSystemRadioButton);
    buttonGroup->addButton(ui->remoteSystemRadioButton);

    connect(ui->urlComboBox,            SIGNAL(activated(int)),             this,   SLOT(at_urlComboBox_activated(int)));
    connect(ui->urlComboBox->lineEdit(),&QLineEdit::editingFinished,        this,   &QnMergeSystemsDialog::at_urlComboBox_editingFinished);
    connect(ui->urlComboBox->lineEdit(),SIGNAL(returnPressed()),            ui->passwordEdit,   SLOT(setFocus()));
    connect(ui->passwordEdit,           &QLineEdit::returnPressed,          this,   &QnMergeSystemsDialog::at_testConnectionButton_clicked);
    connect(ui->testConnectionButton,   &QPushButton::clicked,              this,   &QnMergeSystemsDialog::at_testConnectionButton_clicked);
    connect(m_mergeButton,              &QPushButton::clicked,              this,   &QnMergeSystemsDialog::at_mergeButton_clicked);

    connect(m_mergeTool,                &QnMergeSystemsTool::systemFound,   this,   &QnMergeSystemsDialog::at_mergeTool_systemFound);
    connect(m_mergeTool,                &QnMergeSystemsTool::mergeFinished, this,   &QnMergeSystemsDialog::at_mergeTool_mergeFinished);

    updateKnownSystems();
    updateConfigurationBlock();
}

QnMergeSystemsDialog::~QnMergeSystemsDialog() {}

QUrl QnMergeSystemsDialog::url() const {
    /* filter unnecessary information from the URL */
    QUrl enteredUrl = QUrl::fromUserInput(ui->urlComboBox->currentText());
    QUrl url;
    url.setScheme(enteredUrl.scheme());
    url.setHost(enteredUrl.host());
    url.setPort(enteredUrl.port());
    return url;
}

QString QnMergeSystemsDialog::password() const {
    return ui->passwordEdit->text();
}

void QnMergeSystemsDialog::updateKnownSystems() {
    ui->urlComboBox->clear();

    foreach (const QnResourcePtr &resource, qnResPool->getAllIncompatibleResources()) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QString url = server->getApiUrl();
        QString label = getResourceName(server);
        QString systemName = server->getSystemName();
        if (!systemName.isEmpty())
            label += lit(" (%1)").arg(systemName);

        ui->urlComboBox->addItem(label, url);
    }

    ui->urlComboBox->setCurrentText(QString());

    ui->currentSystemLabel->setText(tr("You are about to merge the current system %1 with the system").arg(qnCommon->localSystemName()));
    ui->currentSystemRadioButton->setText(tr("%1 (current)").arg(qnCommon->localSystemName()));
}

void QnMergeSystemsDialog::updateErrorLabel(const QString &error) {
    ui->errorLabel->setText(error);
}

void QnMergeSystemsDialog::updateConfigurationBlock() {
    bool found = !m_discoverer.isNull();
    ui->configurationWidget->setEnabled(found);
    m_mergeButton->setEnabled(found);
    if (found)
        m_mergeButton->setFocus();
}

void QnMergeSystemsDialog::at_urlComboBox_activated(int index) {
    if (index == -1)
        return;

    ui->urlComboBox->setCurrentText(ui->urlComboBox->itemData(index).toString());
    ui->passwordEdit->setFocus();
}

void QnMergeSystemsDialog::at_urlComboBox_editingFinished() {
    QUrl url = QUrl::fromUserInput(ui->urlComboBox->currentText());
    if (url.port() == -1)
        url.setPort(DEFAULT_APPSERVER_PORT);
    ui->urlComboBox->setCurrentText(url.toString());
}

void QnMergeSystemsDialog::at_testConnectionButton_clicked() {
    m_discoverer.clear();
    m_url.clear();
    m_user.clear();
    m_password.clear();

    QUrl url = QUrl::fromUserInput(ui->urlComboBox->currentText());
    QString password = ui->passwordEdit->text();

    if ((url.scheme() != lit("http") && url.scheme() != lit("https")) || url.host().isEmpty()) {
        updateErrorLabel(tr("The URL is invalid."));
        updateConfigurationBlock();
        return;
    }

    if (password.isEmpty()) {
        updateErrorLabel(tr("The password cannot be empty."));
        updateConfigurationBlock();
        return;
    }

    m_url = url;
    m_user = lit("admin");
    m_password = password;
    m_mergeTool->pingSystem(m_url, m_user, m_password);
    ui->buttonBox->showProgress(tr("testing..."));
}

void QnMergeSystemsDialog::at_mergeButton_clicked() {
    if (!m_discoverer)
        return;

    bool ownSettings = ui->currentSystemRadioButton->isChecked();
    ui->credentialsGroupBox->setEnabled(false);
    ui->configurationWidget->setEnabled(false);
    m_mergeButton->setEnabled(false);

    m_mergeTool->mergeSystem(m_discoverer, m_url, m_user, m_password, ownSettings);
    ui->buttonBox->showProgress(tr("merging systems..."));
}

void QnMergeSystemsDialog::at_mergeTool_systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode) {
    ui->buttonBox->hideProgress();

    switch (errorCode) {
    case QnMergeSystemsTool::NoError: {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleInformation.id).dynamicCast<QnMediaServerResource>();
        if (server && server->getStatus() == Qn::Online && moduleInformation.systemName == qnCommon->localSystemName()) {
            if (m_url.host() == lit("localhost") || m_url.host() == lit("127.0.0.1"))
                updateErrorLabel(tr("Use a specific hostname or IP address rather than %1.").arg(m_url.host()));
            else
                updateErrorLabel(tr("This is the current system URL."));
            break;
        }
        m_discoverer = discoverer;
        ui->remoteSystemLabel->setText(moduleInformation.systemName);
        m_mergeButton->setText(tr("Merge with %1").arg(moduleInformation.systemName));
        m_mergeButton->show();
        ui->remoteSystemRadioButton->setText(moduleInformation.systemName);
        updateErrorLabel(QString());
        break;
    }
    case QnMergeSystemsTool::AuthentificationError:
        updateErrorLabel(tr("The password is invalid."));
        break;
    case QnMergeSystemsTool::VersionError:
        updateErrorLabel(tr("The found system %1 has an incompatible version %2.").arg(moduleInformation.systemName).arg(moduleInformation.version.toString()));
        break;
    default:
        updateErrorLabel(tr("The system was not found."));
        break;
    }

    updateConfigurationBlock();
}

void QnMergeSystemsDialog::at_mergeTool_mergeFinished(int errorCode, const QnModuleInformation &moduleInformation) {
    ui->buttonBox->hideProgress();
    ui->credentialsGroupBox->setEnabled(true);

    if (errorCode == QnMergeSystemsTool::NoError) {
        m_mergeButton->hide();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->hide();
        ui->buttonBox->button(QDialogButtonBox::Close)->show();
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        ui->stackedWidget->setCurrentIndex(1);
    } else {
        QString message;

        switch (errorCode) {
        case QnMergeSystemsTool::AuthentificationError:
            message = tr("The password is invalid.");
            break;
        case QnMergeSystemsTool::VersionError:
            updateErrorLabel(tr("The found system %1 has an incompatible version %2.").arg(moduleInformation.systemName).arg(moduleInformation.version.toString()));
            break;
        case QnMergeSystemsTool::BackupError:
            message = tr("Could not create a backup of the server database.");
            break;
        case QnMergeSystemsTool::NotFoundError:
            message = tr("System was not found.");
            break;
        default:
            break;
        }

        if (!message.isEmpty())
            message.prepend(lit("\n"));

        QMessageBox::critical(this, tr("Error"), tr("Cannot merge systems.") + message);

        updateConfigurationBlock();
    }
}

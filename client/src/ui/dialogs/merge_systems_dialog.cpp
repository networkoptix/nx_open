#include "merge_systems_dialog.h"
#include "ui_merge_systems_dialog.h"

#include <QtCore/QUrl>
#include <QtWidgets/QButtonGroup>

#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "ui/common/ui_resource_name.h"
#include "ui/style/warning_style.h"
#include "utils/merge_systems_tool.h"

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
    connect(ui->passwordEdit,           &QLineEdit::returnPressed,          this,   &QnMergeSystemsDialog::at_testConnectionButton_clicked);
    connect(ui->testConnectionButton,   &QPushButton::clicked,              this,   &QnMergeSystemsDialog::at_testConnectionButton_clicked);
    connect(m_mergeButton,              &QPushButton::clicked,              this,   &QnMergeSystemsDialog::at_mergeButton_clicked);

    connect(m_mergeTool,                &QnMergeSystemsTool::systemFound,   this,   &QnMergeSystemsDialog::at_mergeTool_systemFound);

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
//    ui->errorLabel->setVisible(!error.isEmpty());
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

void QnMergeSystemsDialog::at_testConnectionButton_clicked() {
    m_discoverer.clear();
    m_url.clear();
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
    m_password = password;
    m_mergeTool->pingSystem(url, password);
    ui->buttonBox->showProgress(tr("testing..."));
}

void QnMergeSystemsDialog::at_mergeButton_clicked() {
    if (!m_discoverer)
        return;

    bool ownSettings = ui->currentSystemRadioButton->isChecked();
    ui->credentialsGroupBox->setEnabled(false);
    ui->configurationWidget->setEnabled(false);
    m_mergeButton->setEnabled(false);

    m_discoverer->apiConnection()->mergeSystemAsync(m_url, m_password, ownSettings, this, SLOT(at_mergeTool_mergeFinished(int)));
    ui->buttonBox->showProgress(tr("merging systems..."));
}

void QnMergeSystemsDialog::at_mergeTool_systemFound(const QnModuleInformation &moduleInformation, const QnMediaServerResourcePtr &discoverer, int errorCode) {
    ui->buttonBox->hideProgress();

    switch (errorCode) {
    case QnMergeSystemsTool::NoError:
        if (qnResPool->getResourceById(moduleInformation.id)) {
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

void QnMergeSystemsDialog::at_mergeTool_mergeFinished(int errorCode) {
    ui->buttonBox->hideProgress();
    ui->credentialsGroupBox->setEnabled(true);

    if (errorCode == QnMergeSystemsTool::NoError) {
        m_mergeButton->hide();
        ui->buttonBox->button(QDialogButtonBox::Cancel)->hide();
        ui->buttonBox->button(QDialogButtonBox::Close)->show();
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        ui->stackedWidget->setCurrentIndex(1);
    } else {
        updateConfigurationBlock();
    }
}

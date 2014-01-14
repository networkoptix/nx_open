#include "connection_testing_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtWidgets/QDataWidgetMapper>
#include <QtWidgets/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QPushButton>

#include <api/session_manager.h>
#include <utils/common/warnings.h>
#include <core/resource/resource.h>

#include <client/client_settings.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include "version.h"

QnConnectionTestingDialog::QnConnectionTestingDialog(const QUrl &url, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionTestingDialog),
    m_timeoutTimer(this),
    m_url(url)
{
    QUrl urlNoPassword(url);
    urlNoPassword.setPassword(QString());
    qnDebug("Testing connectivity for URL '%1'.", urlNoPassword.toString());

    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(false);

    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(100);

    testSettings();

    connect(&m_timeoutTimer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_timeoutTimer.setInterval(100);
    m_timeoutTimer.setSingleShot(false);
    m_timeoutTimer.start();
}

QnConnectionTestingDialog::~QnConnectionTestingDialog()
{
}

void QnConnectionTestingDialog::accept()
{
    QDialog::accept();
}


void QnConnectionTestingDialog::reject()
{
    QDialog::reject();
}


void QnConnectionTestingDialog::timeout()
{
    if (ui->progressBar->value() != ui->progressBar->maximum())
    {
        ui->progressBar->setValue(ui->progressBar->value() + 1);
        return;
    }

    m_timeoutTimer.stop();
    updateUi(false, tr("Request timed out."), Qn::Login_Help);
}

void QnConnectionTestingDialog::testResults(int status, QnConnectInfoPtr connectInfo, int requestHandle)
{
    Q_UNUSED(requestHandle)

    if (!m_timeoutTimer.isActive())
        return;

    m_timeoutTimer.stop();

    QnCompatibilityChecker remoteChecker(connectInfo->compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size())
        compatibilityChecker = &remoteChecker;
    else
        compatibilityChecker = &localChecker;

    ui->progressBar->setValue(ui->progressBar->maximum());

    bool success = true;
    QString detail;
    int helpTopicId = -1;

    bool compatibleProduct = qnSettings->isDevMode() || connectInfo->brand.isEmpty()
            || connectInfo->brand == QLatin1String(QN_PRODUCT_NAME_SHORT);

    if (status == 202) {
        success = false;
        detail = tr("Login or password you have entered are incorrect, please try again.");
        helpTopicId = Qn::Login_Help;
    } else if (status != 0) {
        success = false;
        detail = tr("Connection to the Enterprise Controller could not be established.\n"\
                    "Connection details that you have entered are incorrect, please try again.\n\n"\
                    "If this error persists, please contact your VMS administrator.");
        helpTopicId = Qn::Login_Help;
    } else if (!compatibleProduct) {
        success = false;
        detail = tr("You are trying to connect to incompatible Enterprise Controller.");
        helpTopicId = Qn::Login_Help;
    } else if (!compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(QN_ENGINE_VERSION), QLatin1String("ECS"), connectInfo->version)) {
        QnSoftwareVersion minSupportedVersion("1.4");

        if (connectInfo->version < minSupportedVersion) {
            detail = tr("Enterprise Controller has a different version:\n"\
                        " - Client version: %1.\n"\
                        " - EC version: %2.\n"\
                        "Compatibility mode for versions lower than %3 is not supported.")
                    .arg(QLatin1String(QN_ENGINE_VERSION))
                    .arg(connectInfo->version.toString())
                    .arg(minSupportedVersion.toString());
            success = false;
            helpTopicId = Qn::VersionMismatch_Help;
        } else {
            detail = tr("Enterprise Controller has a different version:\n"\
                        " - Client version: %1.\n"\
                        " - EC version: %2.\n"\
                        "You will be asked to restart the client in compatibility mode.")
                    .arg(QLatin1String(QN_ENGINE_VERSION))
                    .arg(connectInfo->version.toString());
            helpTopicId = Qn::VersionMismatch_Help;
        }
    }

    updateUi(success, detail, helpTopicId);
}

void QnConnectionTestingDialog::testSettings()
{
    m_connection = QnAppServerConnectionFactory::createConnection(m_url);
    m_connection->testConnectionAsync(this, SLOT(testResults(int,QnConnectInfoPtr,int)));
}

void QnConnectionTestingDialog::updateUi(bool success, const QString &details, int helpTopicId) {
    ui->statusLabel->setText(success ? tr("Success") : tr("Failed"));
    ui->descriptionLabel->setText(details);

    setHelpTopic(this, helpTopicId);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(false);
}



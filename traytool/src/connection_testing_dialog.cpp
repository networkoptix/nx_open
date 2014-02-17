#include "connection_testing_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtGui/QStandardItemModel>

#include <QtConcurrent/QtConcurrentRun>

#include <QtNetwork/QTcpSocket>

#include <QtWidgets/QDataWidgetMapper>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <api/session_manager.h>
#include <api/app_server_connection.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>

#include <utils/common/warnings.h>

#ifdef Q_OS_MACX
    #include <utils/network/icmp_mac.h>
#endif

#include "version.h"

QnConnectionTestingDialog::QnConnectionTestingDialog( QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionTestingDialog),
    m_timeoutTimer(new QTimer(this))
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(false);

    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(100);

    connect(m_timeoutTimer, &QTimer::timeout, this, &QnConnectionTestingDialog::tick);
    m_timeoutTimer->setInterval(100);
    m_timeoutTimer->setSingleShot(false);
}

void QnConnectionTestingDialog::testEnterpriseController(const QUrl &url) {
    if (!QnSessionManager::instance()->isReady())
        QnSessionManager::instance()->start();

    QUrl urlNoPassword(url);
    urlNoPassword.setPassword(QString());
    qnDebug("Testing connectivity for URL '%1'.", urlNoPassword.toString());

    // TODO: #EC2 #Elric
    //QnAppServerConnectionFactory::createConnection(url)->testConnectionAsync(this, SLOT(at_ecConnection_result(int, QnConnectionInfoPtr, int)));

    m_timeoutTimer->start();
}

QnConnectionTestingDialog::~QnConnectionTestingDialog() {}

void QnConnectionTestingDialog::tick() {
    if (ui->progressBar->value() != ui->progressBar->maximum()) {
        ui->progressBar->setValue(ui->progressBar->value() + 1);
        return;
    }

    m_timeoutTimer->stop();
    updateUi(false, tr("Request timed out."));
}

void QnConnectionTestingDialog::at_ecConnection_result(int status, QnConnectionInfoPtr connectionInfo, int requestHandle) {
    Q_UNUSED(requestHandle)

    if (!m_timeoutTimer->isActive())
        return;
    m_timeoutTimer->stop();
    ui->progressBar->setValue(ui->progressBar->maximum());

    QnCompatibilityChecker remoteChecker(connectionInfo->compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size())
        compatibilityChecker = &remoteChecker;
    else
        compatibilityChecker = &localChecker;


    bool success = true;
    QString detail;

    bool compatibleProduct = connectionInfo->brand.isEmpty() || connectionInfo->brand == QLatin1String(QN_PRODUCT_NAME_SHORT);

    if (status == 202) {
        success = false;
        detail = tr("Login or password you have entered are incorrect, please try again.");
    } else if (status != 0) {
        success = false;
        detail = tr("Connection to the Enterprise Controller could not be established.\n"\
                    "Connection details that you have entered are incorrect, please try again.\n\n"\
                    "If this error persists, please contact your VMS administrator.");
    } else if (!compatibleProduct) {
        success = false;
        detail = tr("You are trying to connect to incompatible Enterprise Controller.");
    } else if (!compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(QN_ENGINE_VERSION), QLatin1String("ECS"), connectionInfo->version)) {
        QnSoftwareVersion minSupportedVersion("1.4");

        if (connectionInfo->version < minSupportedVersion) {
            detail = tr("Enterprise Controller has a different version:\n"\
                        " - Client version: %1.\n"\
                        " - EC version: %2.\n"\
                        "Compatibility mode for versions lower than %3 is not supported.")
                    .arg(QLatin1String(QN_ENGINE_VERSION))
                    .arg(connectionInfo->version.toString())
                    .arg(minSupportedVersion.toString());
            success = false;
        } else {
            detail = tr("Enterprise Controller has a different version:\n"\
                        " - Client version: %1.\n"\
                        " - EC version: %2.\n"\
                        "You will be asked to restart the client in compatibility mode.")
                    .arg(QLatin1String(QN_ENGINE_VERSION))
                    .arg(connectionInfo->version.toString());
        }
    }

    updateUi(success, detail);
}

void QnConnectionTestingDialog::updateUi(bool success, const QString &details) {
    ui->statusLabel->setText(success ? tr("Success") : tr("Failed"));
    ui->descriptionLabel->setText(details);
    ui->descriptionLabel->setVisible(!details.isEmpty());

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(false);
}



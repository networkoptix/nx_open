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

#include <common/common_module.h>

#include <client/client_settings.h>

#include <nx_ec/ec_proto_version.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/app_info.h>
#include <utils/common/warnings.h>

#include "compatibility.h"


QnConnectionTestingDialog::QnConnectionTestingDialog( QWidget *parent) :
    QnButtonBoxDialog(parent),
    ui(new Ui::ConnectionTestingDialog),
    m_timeoutTimer(new QTimer(this))
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(false);

    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(100);

    connect(m_timeoutTimer, &QTimer::timeout, this, &QnConnectionTestingDialog::tick);
    m_timeoutTimer->setInterval(100);
    m_timeoutTimer->setSingleShot(false);
}

void QnConnectionTestingDialog::testEnterpriseController(const QUrl &url) {
    QUrl urlNoPassword(url);
    urlNoPassword.setPassword(QString());
    qnDebug("Testing connectivity for URL '%1'.", urlNoPassword.toString());

    setHelpTopic(this, Qn::Login_Help);

    QnAppServerConnectionFactory::ec2ConnectionFactory()->testConnection(
        url,
        this,
        &QnConnectionTestingDialog::at_ecConnection_result );

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

void QnConnectionTestingDialog::at_ecConnection_result(int reqID, ec2::ErrorCode errorCode, QnConnectionInfo connectionInfo) {
    Q_UNUSED(reqID)

    if (!m_timeoutTimer->isActive())
        return;
    m_timeoutTimer->stop();
    ui->progressBar->setValue(ui->progressBar->maximum());

    QnCompatibilityChecker remoteChecker(connectionInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size())
        compatibilityChecker = &remoteChecker;
    else
        compatibilityChecker = &localChecker;

    //TODO #GDM almost same code exists in QnConnectionDiagnosticsHelper::validateConnection

    bool success = true;
    QString detail;
    int helpTopicId = -1;

    bool compatibleProduct = qnSettings->isDevMode() || connectionInfo.brand.isEmpty()
            || connectionInfo.brand == QnAppInfo::productNameShort();

     if (errorCode == ec2::ErrorCode::unauthorized) {
        success = false;
        detail = tr("Login or password you have entered are incorrect, please try again.");
        helpTopicId = Qn::Login_Help;
    } else if (errorCode != ec2::ErrorCode::ok) {
        success = false;
        detail = tr("Connection to the Server could not be established.\n"\
                    "Connection details that you have entered are incorrect, please try again.\n\n"\
                    "If this error persists, please contact your VMS administrator.");
        helpTopicId = Qn::Login_Help;
    } else if (!compatibleProduct) {
        success = false;
        detail = tr("You are trying to connect to incompatible Server.");
        helpTopicId = Qn::Login_Help;
    } else if (!compatibilityChecker->isCompatible(QLatin1String("Client"), qnCommon->engineVersion(), QLatin1String("ECS"), connectionInfo.version)) {
        QnSoftwareVersion minSupportedVersion("1.4");

        if (connectionInfo.version < minSupportedVersion) {
            detail = tr("Server has a different version:\n"\
                        " - Client version: %1.\n"\
                        " - Server version: %2.\n"\
                        "Compatibility mode for versions lower than %3 is not supported.")
                    .arg(qnCommon->engineVersion().toString())
                    .arg(connectionInfo.version.toString())
                    .arg(minSupportedVersion.toString());
            success = false;
            helpTopicId = Qn::VersionMismatch_Help;
        } else {
            detail = tr("Server has a different version:\n"\
                        " - Client version: %1.\n"\
                        " - Server version: %2.\n"\
                        "You will be asked to restart the client in compatibility mode.")
                    .arg(qnCommon->engineVersion().toString())
                    .arg(connectionInfo.version.toString());
            helpTopicId = Qn::VersionMismatch_Help;
        }
    } else if (connectionInfo.nxClusterProtoVersion != nx_ec::EC2_PROTO_VERSION) {
        QString olderComponent = connectionInfo.nxClusterProtoVersion < nx_ec::EC2_PROTO_VERSION
            ? tr("Server")
            : tr("Client");
        detail = tr("Server has a different version:\n"
            " - Client version: %1.\n"
            " - Server version: %2.\n"
            "These versions are not compatible. Please update your %3" // TODO: #TR after string freeze replace with "You will be asked to update your %3."
            ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()).arg(olderComponent);
        helpTopicId = Qn::VersionMismatch_Help;
    }

    updateUi(success, detail, helpTopicId);
}

void QnConnectionTestingDialog::updateUi(bool success, const QString &details, int helpTopicId) {
    ui->statusLabel->setText(success ? tr("Success") : tr("Failed"));
    ui->descriptionLabel->setText(details);
    ui->descriptionLabel->setVisible(!details.isEmpty());

    if (helpTopicId >= 0)
        setHelpTopic(this, helpTopicId);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(false);
}



#include "connection_testing_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtGui/QStandardItemModel>

#include <QtConcurrent/QtConcurrentRun>

#include <QtWidgets/QDataWidgetMapper>
#include <QtWidgets/QPushButton>

#include <api/session_manager.h>
#include <api/app_server_connection.h>

#include <client_core/client_core_module.h>

#include <core/resource/resource.h>

#include <common/common_module.h>

#include <client/client_settings.h>

#include <nx_ec/ec_proto_version.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/app_info.h>
#include <utils/common/warnings.h>
#include <utils/connection_diagnostics_helper.h>

namespace
{
    const int timerIntervalMs = 100;

    // TODO: #GDM move timeout constant to more common module
    const int connectionTimeoutMs = 30 * 1000; //ec2::RESPONSE_WAIT_TIMEOUT_MS;
}


QnConnectionTestingDialog::QnConnectionTestingDialog( QWidget *parent)
    : QnButtonBoxDialog(parent)
    , ui(new Ui::ConnectionTestingDialog)
    , m_timeoutTimer(new QTimer(this))
    , m_connectButton(new QPushButton(tr("Connect"), this))
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(false);
    ui->buttonBox->addButton(m_connectButton, QDialogButtonBox::HelpRole);

    m_connectButton->setVisible(false);
    connect(m_connectButton, &QPushButton::clicked, this, [this] {
        emit connectRequested();
        accept();
    });

    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(connectionTimeoutMs / timerIntervalMs);

    connect(m_timeoutTimer, &QTimer::timeout, this, &QnConnectionTestingDialog::tick);
    m_timeoutTimer->setInterval(timerIntervalMs);
    m_timeoutTimer->setSingleShot(false);
}

void QnConnectionTestingDialog::testConnection(const nx::utils::Url &url) {
    setHelpTopic(this, Qn::Login_Help);
    qnClientCoreModule->connectionFactory()->testConnection(
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
    updateUi(false, tr("Request timeout"));
}

void QnConnectionTestingDialog::at_ecConnection_result(int reqID, ec2::ErrorCode errorCode,
    const QnConnectionInfo &connectionInfo)
{
    Q_UNUSED(reqID)

    if (!m_timeoutTimer->isActive())
        return;
    m_timeoutTimer->stop();
    ui->progressBar->setValue(ui->progressBar->maximum());

    auto testResult = QnConnectionDiagnosticsHelper::validateConnectionTest(
        connectionInfo, errorCode);

    updateUi(testResult.result == Qn::SuccessConnectionResult,
        testResult.details, testResult.helpTopicId);
}

void QnConnectionTestingDialog::updateUi(bool success, const QString &details, int helpTopicId) {
    ui->statusLabel->setText(success ? tr("Success") : tr("Test Failed"));
    ui->descriptionLabel->setText(details);
    ui->descriptionLabel->setVisible(!details.isEmpty());

    if (helpTopicId != Qn::Empty_Help)
        setHelpTopic(this, helpTopicId);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(false);
    m_connectButton->setVisible(success);
}



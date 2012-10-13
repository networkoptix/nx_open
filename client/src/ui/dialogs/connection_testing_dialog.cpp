#include "connection_testing_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QPushButton>

#include <api/session_manager.h>
#include <utils/common/warnings.h>
#include <core/resource/resource.h>

#include "utils/settings.h"
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
    updateUi(false);
}

void QnConnectionTestingDialog::oldHttpTestResults(int status, QByteArray errorString, QByteArray data, int handle)
{
    Q_UNUSED(errorString)
    Q_UNUSED(data)
    Q_UNUSED(handle)

    if (status == 204 && m_timeoutTimer.isActive())
    {
        m_timeoutTimer.stop();
        updateUi(false);
    }
}

void QnConnectionTestingDialog::testResults(int status, const QByteArray &errorString, QnConnectInfoPtr connectInfo, int requestHandle)
{
    Q_UNUSED(requestHandle)
    Q_UNUSED(errorString)

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
    updateUi(!status && compatibilityChecker->isCompatible(QLatin1String("Client"), QLatin1String(QN_ENGINE_VERSION), QLatin1String("ECS"), connectInfo->version));
}

void QnConnectionTestingDialog::testSettings()
{
    m_connection = QnAppServerConnectionFactory::createConnection(m_url);
    m_connection->testConnectionAsync(this, SLOT(testResults(int,QByteArray,QnConnectInfoPtr,int)));

    QUrl httpUrl;
    httpUrl.setHost(m_url.host());
    httpUrl.setPort(m_url.port());
    httpUrl.setScheme(QLatin1String("http"));
    httpUrl.setUserName(QString());
    httpUrl.setPassword(QString());
    QnSessionManager::instance()->sendAsyncGetRequest(httpUrl, QLatin1String("resourceEx"), this, SLOT(oldHttpTestResults(int,QByteArray,QByteArray,int)));
}

void QnConnectionTestingDialog::updateUi(bool success){
    ui->statusLabel->setText(success ? tr("Success") : tr("Failed"));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(false);
}



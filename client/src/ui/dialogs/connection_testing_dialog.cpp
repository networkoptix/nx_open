#include "connection_testing_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <api/session_manager.h>
#include <utils/common/warnings.h>
#include <core/resource/resource.h>

#include "utils/settings.h"

QnConnectionTestingDialog::QnConnectionTestingDialog(const QUrl &url, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionTestingDialog),
    m_timeoutTimer(this),
    m_url(url)
{
    QUrl urlNoPassword(url);
    urlNoPassword.setPassword("");
    qnDebug("Testing connectivity for URL '%1'.", urlNoPassword.toString());

    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

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

void QnConnectionTestingDialog::timeout()
{
    if (ui->progressBar->value() != ui->progressBar->maximum())
    {
        ui->progressBar->setValue(ui->progressBar->value() + 1);
        return;
    }

    m_timeoutTimer.stop();

    ui->statusLabel->setText("Failed");
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

void QnConnectionTestingDialog::oldHttpTestResults(int status, QByteArray errorString, QByteArray data, int handle)
{
	Q_UNUSED(handle);

	if (status == 204)
	{
		if (m_timeoutTimer.isActive())
			m_timeoutTimer.stop();
		else
			return;

		ui->statusLabel->setText("Failed");
	}
}

void QnConnectionTestingDialog::testResults(int status, const QByteArray &errorString, QnConnectInfoPtr connectInfo, int requestHandle)
{
    Q_UNUSED(requestHandle)
	Q_UNUSED(errorString)

    if (m_timeoutTimer.isActive()) {
        m_timeoutTimer.stop();
    } else {
        return;
    }

    QnCompatibilityChecker remoteChecker(connectInfo->compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size())
        compatibilityChecker = &remoteChecker;
    else
        compatibilityChecker = &localChecker;

    ui->progressBar->setValue(ui->progressBar->maximum());
    if (status || !compatibilityChecker->isCompatible("Client", qApp->applicationVersion(), "ECS", connectInfo->version)) {
        ui->statusLabel->setText("Failed");
    } else {
        ui->statusLabel->setText("Success");
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void QnConnectionTestingDialog::testSettings()
{
    m_connection = QnAppServerConnectionFactory::createConnection(m_url);
    m_connection->testConnectionAsync(this, SLOT(testResults(int,QByteArray,QnConnectInfoPtr,int)));

	QUrl httpUrl;
	httpUrl.setHost(m_url.host());
	httpUrl.setPort(m_url.port());
	httpUrl.setScheme("http");
	httpUrl.setUserName("");
	httpUrl.setPassword("");
	QnSessionManager::instance()->sendAsyncGetRequest(httpUrl, "resourceEx", this, SLOT(oldHttpTestResults(int,QByteArray,QByteArray,int)));
}

void QnConnectionTestingDialog::accept()
{
    QDialog::accept();
}


void QnConnectionTestingDialog::reject()
{
    QDialog::reject();
}


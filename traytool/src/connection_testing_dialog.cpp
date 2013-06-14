#include "connection_testing_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QPushButton>

#include <api/session_manager.h>
#include <utils/common/warnings.h>

//#include "client/client_settings.h"
#include "version.h"

QnConnectionTestingDialog::QnConnectionTestingDialog(const QUrl &url, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionTestingDialog),
    m_timeoutTimer(this),
    m_url(url)
{
    if (!QnSessionManager::instance()->isReady())
        QnSessionManager::instance()->start();

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

void QnConnectionTestingDialog::oldHttpTestResults(const QnHTTPRawResponse& response, int handle)
{
    Q_UNUSED(handle)

    if (m_timeoutTimer.isActive())
        m_timeoutTimer.stop();
    updateUi(response.status == 0);
}

void QnConnectionTestingDialog::testSettings()
{
    QnRequestParamList params;
    params.append(QnRequestParam("ping", "1"));
    QnSessionManager::instance()->sendAsyncGetRequest(m_url, QLatin1String("connect"), this, SLOT(oldHttpTestResults(const QnHTTPRawResponse&, int)));
}

void QnConnectionTestingDialog::updateUi(bool success){
    ui->progressBar->setValue(100);
    ui->statusLabel->setText(success ? tr("Success") : tr("Failed"));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setVisible(true);
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setVisible(false);
}



#include "login_dialog.h"
#include "ui_connection_testing_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <api/SessionManager.h>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "connection_testing_dialog.h"

#include "utils/settings.h"

ConnectionTestingDialog::ConnectionTestingDialog(QWidget *parent, QUrl url) :
    QDialog(parent),
    ui(new Ui::ConnectionTestingDialog),
    m_timeoutTimer(this),
    m_url(url)
{
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

ConnectionTestingDialog::~ConnectionTestingDialog()
{
}

void ConnectionTestingDialog::timeout()
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

void ConnectionTestingDialog::testResults(int status, const QByteArray &data, const QByteArray& errorString, int requestHandle)
{
    Q_UNUSED(data)
    Q_UNUSED(requestHandle)
	Q_UNUSED(errorString)

    if (m_timeoutTimer.isActive()) {
        m_timeoutTimer.stop();
    } else {
        return;
    }

    ui->progressBar->setValue(ui->progressBar->maximum());
    if (status) {
        ui->statusLabel->setText("Failed");
    } else {
        ui->statusLabel->setText("Success");
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void ConnectionTestingDialog::testSettings()
{
    m_connection = QnAppServerConnectionFactory::createConnection(m_url);
    m_connection->testConnectionAsync(this, SLOT(testResults(int,QByteArray,QByteArray,int)));
}

void ConnectionTestingDialog::accept()
{
    QDialog::accept();
}


void ConnectionTestingDialog::reject()
{
    QDialog::reject();
}


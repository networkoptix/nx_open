#include "licensemanagerwidget.h"
#include "ui_licensemanagerwidget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

LicenseManagerWidget::LicenseManagerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LicenseManagerWidget)
{
    ui->setupUi(this);

    m_licenses = qnLicensePool->getLicenses();
    ui->licenseWidget->setManager(this);
    ui->licenseWidget->setHardwareId(m_licenses.hardwareId());

    updateControls();

    connect(qnLicensePool, SIGNAL(licensesChanged()), this, SLOT(licensesChanged()));
}

LicenseManagerWidget::~LicenseManagerWidget()
{
}

void LicenseManagerWidget::licensesChanged()
{
    m_licenses = qnLicensePool->getLicenses();
    updateControls();
}

void LicenseManagerWidget::licensesReceived(int status, const QByteArray& errorString, QnLicenseList licenses, int handle)
{
    if (status != 0)
    {
        QMessageBox::information(this, tr("License Activation"),
                                 tr("There was a problem activating your license."));

        return;
    }

    m_licenses.append(licenses);
    qnLicensePool->addLicenses(licenses);

    updateControls();

    QMessageBox::information(this, tr("License Activation"),
                             tr("License was succesfully activated."));
}

void LicenseManagerWidget::updateControls()
{
    for (int i = 0; i < ui->gridLicenses->rowCount(); i++) {
        ui->gridLicenses->removeRow(i);
    }

    foreach(const QnLicensePtr& license, m_licenses) {
        int row = ui->gridLicenses->rowCount();
        ui->gridLicenses->insertRow(row);
        ui->gridLicenses->setItem(row, 0, new QTableWidgetItem(license->name(), QTableWidgetItem::Type));
        ui->gridLicenses->setItem(row, 1, new QTableWidgetItem(QString::number(license->cameraCount()), QTableWidgetItem::Type));
        ui->gridLicenses->setItem(row, 2, new QTableWidgetItem(license->key(), QTableWidgetItem::Type));
    }

    if (!m_licenses.isEmpty()) {
        QPalette palette = ui->infoLabel->palette();
        palette.setColor(QPalette::Foreground, parentWidget() ? parentWidget()->palette().color(QPalette::Foreground) : Qt::black);
        ui->infoLabel->setPalette(palette);
        ui->infoLabel->setText(tr("You have a valid License installed"));
    } else {
        QPalette palette = ui->infoLabel->palette();
        palette.setColor(QPalette::Foreground, Qt::red);
        ui->infoLabel->setPalette(palette);
        ui->infoLabel->setText(tr("You do not have a valid License installed"));
    }
}

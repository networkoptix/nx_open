#include "licensemanagerwidget.h"
#include "ui_licensemanagerwidget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "core/resourcemanagment/resource_pool.h"

LicenseManagerWidget::LicenseManagerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LicenseManagerWidget)
{
    ui->setupUi(this);

    ui->detailsButton->setEnabled(false);
    ui->gridLicenses->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_licenses = qnLicensePool->getLicenses();

    ui->licenseWidget->setManager(this);
    ui->licenseWidget->setHardwareId(m_licenses.hardwareId());

    updateControls();

    connect(ui->detailsButton, SIGNAL(clicked()), this, SLOT(licenseDetailsButtonClicked()));
    connect(qnLicensePool, SIGNAL(licensesChanged()), this, SLOT(licensesChanged()));
}

LicenseManagerWidget::~LicenseManagerWidget()
{
}

void LicenseManagerWidget::licenseDetailsButtonClicked()
{
    const QnLicensePtr license = m_licenses.licenses().at(ui->gridLicenses->selectionModel()->selectedRows().front().row());

    QString details = tr("<b>Generic:</b><br />\n"
                         "License Qwner: %1<br />\n"
                         "Serial Key: %2<br />\n"
                         "Locked to Hardware ID: %3<br />\n"
                         "<br />\n"
                         "<b>Features:</b><br />\n"
                         "Archive Streams Allowed: %4")
                      .arg(license->name().constData())
                      .arg(license->key().constData())
                      .arg(license->hardwareId().constData())
                      .arg(license->cameraCount());
    QMessageBox::information(this, tr("License Details"), details);
}

void LicenseManagerWidget::licensesChanged()
{
    m_licenses = qnLicensePool->getLicenses();
    updateControls();
}

void LicenseManagerWidget::licensesReceived(int status, const QByteArray& errorString, QnLicenseList licenses, int handle)
{
    if (status != 0 || licenses.isEmpty())
    {
        QMessageBox::information(this, tr("License Activation"), tr("There was a problem activating your license."));
        return;
    }

    QnLicensePtr license = licenses.licenses().front();
    QString message = m_licenses.haveLicenseKey(license->key()) ? tr("This license is already activated.") : tr("License was succesfully activated.");

    m_licenses.append(license);
    qnLicensePool->addLicense(license);

    updateControls();

    QMessageBox::information(this, tr("License Activation"), message);
    ui->licenseWidget->ui->serialKeyEdit->clear();
}

void LicenseManagerWidget::updateControls()
{
    if (m_licenses.hardwareId().isEmpty())
    {
        setEnabled(false);
        return;
    } else {
        setEnabled(true);
    }

    for (int i = 0; i < ui->gridLicenses->rowCount(); i++) {
        ui->gridLicenses->removeRow(i);
    }

    foreach(const QnLicensePtr& license, m_licenses.licenses()) {
        int row = ui->gridLicenses->rowCount();
        ui->gridLicenses->insertRow(row);
        ui->gridLicenses->setItem(row, 0, new QTableWidgetItem(license->name(), QTableWidgetItem::Type));
        ui->gridLicenses->setItem(row, 1, new QTableWidgetItem(QString::number(license->cameraCount()), QTableWidgetItem::Type));
        ui->gridLicenses->setItem(row, 2, new QTableWidgetItem(license->key(), QTableWidgetItem::Type));
    }

    if (!m_licenses.isEmpty()) {
        QPalette palette = ui->infoLabel->palette();
        int totalCameras = m_licenses.totalCameras();
        int usingCameras = qnResPool->activeCameras();
        if (usingCameras > totalCameras)
            palette.setColor(QPalette::Foreground, Qt::red);
        else
            palette.setColor(QPalette::Foreground, parentWidget() ? parentWidget()->palette().color(QPalette::Foreground) : Qt::black);
        ui->infoLabel->setPalette(palette);
        ui->infoLabel->setText(QString(tr("The software is licensed to %1 cameras. Currently using %2.")).arg(totalCameras).arg(usingCameras));
    } else {
        QPalette palette = ui->infoLabel->palette();
        palette.setColor(QPalette::Foreground, Qt::red);
        ui->infoLabel->setPalette(palette);
        if (m_licenses.hardwareId().isEmpty())
            ui->infoLabel->setText(tr("Obtaining licenses from application server..."));
        else
            ui->infoLabel->setText(tr("You do not have a valid License installed. Please activate your commercial or free license."));
    }

    ui->licenseWidget->ui->activateFreeLicenseButton->setVisible(!m_licenses.haveLicenseKey(QnLicense::FREE_LICENSE_KEY));
    ui->licenseWidget->updateControls();
}

void LicenseManagerWidget::gridSelectionChanged()
{
    ui->detailsButton->setEnabled(!ui->gridLicenses->selectionModel()->selectedRows().isEmpty());
}

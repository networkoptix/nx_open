#include "licensemanagerwidget.h"
#include "ui_licensemanagerwidget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <core/resourcemanagment/resource_pool.h>

#include <ui/style/globals.h>


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
                         "License Owner: %1<br />\n"
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
    } else {
        setEnabled(true);
    }

    ui->gridLicenses->clear();

    foreach(const QnLicensePtr& license, m_licenses.licenses()) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setData(0, Qt::DisplayRole, license->name());
        item->setData(1, Qt::DisplayRole, license->cameraCount());
        item->setData(2, Qt::DisplayRole, license->key());
        ui->gridLicenses->addTopLevelItem(item);
    }

    bool useRedLabel = false;

    if (!m_licenses.isEmpty()) {
        int totalCameras = m_licenses.totalCameras();
        int usingCameras = qnResPool->activeCameras();
        ui->infoLabel->setText(QString(tr("The software is licensed to %1 cameras. Currently using %2.")).arg(totalCameras).arg(usingCameras));
        useRedLabel = usingCameras > totalCameras;
    } else {
        if (m_licenses.hardwareId().isEmpty()) {
            ui->infoLabel->setText(tr("Obtaining licenses from Enterprise Controller..."));
            useRedLabel = false;
        } else {
            ui->infoLabel->setText(tr("You do not have a valid License installed. Please activate your commercial or free license."));
            useRedLabel = true;
        }
    }

    if(useRedLabel) {
        QPalette palette = ui->infoLabel->palette();
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
        ui->infoLabel->setPalette(palette);
    } else {
        ui->infoLabel->setPalette(palette());
    }

    ui->licenseWidget->ui->activateFreeLicenseButton->setVisible(!m_licenses.haveLicenseKey(QnLicense::FREE_LICENSE_KEY));
    ui->licenseWidget->updateControls();
}

void LicenseManagerWidget::gridSelectionChanged()
{
    ui->detailsButton->setEnabled(!ui->gridLicenses->selectionModel()->selectedRows().isEmpty());
}

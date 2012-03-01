#include "licensemanagerwidget.h"
#include "ui_licensemanagerwidget.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "licensing/license.h"

LicenseManagerWidget::LicenseManagerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LicenseManagerWidget)
{
    ui->setupUi(this);

    updateControls();
}

LicenseManagerWidget::~LicenseManagerWidget()
{
}

void LicenseManagerWidget::updateControls()
{
    ui->gridLicenses->clear();
    foreach(const QnLicensePtr& license, qnLicensePool->getLicenses()) {
        int row = ui->gridLicenses->rowCount();
        ui->gridLicenses->insertRow(row);
        ui->gridLicenses->setItem(row, 0, new QTableWidgetItem(license->name(), QTableWidgetItem::Type));
        ui->gridLicenses->setItem(row, 1, new QTableWidgetItem(license->key(), QTableWidgetItem::Type));
        ui->gridLicenses->setItem(row, 2, new QTableWidgetItem(QString::number(license->cameraCount()), QTableWidgetItem::Type));
    }

    if (qnLicensePool->haveValidLicense()) {
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

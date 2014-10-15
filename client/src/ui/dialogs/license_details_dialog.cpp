#include "license_details_dialog.h"
#include "ui_license_details_dialog.h"

#include <licensing/license.h>

QnLicenseDetailsDialog::QnLicenseDetailsDialog(const QnLicensePtr &license, QWidget *parent /*= NULL*/):
    base_type(parent),
    ui(new Ui::LicenseDetailsDialog())
{
    ui->setupUi(this);

    if (!license)
        reject();

    ui->licenseTypeLabel->setText(license->displayName());
    ui->licenseKeyLabel->setText(QLatin1String(license->key()));
    ui->licenseHwidLabel->setText(QLatin1String(qnLicensePool->currentHardwareId()));

    QString feature = (license->type() == Qn::LC_VideoWall)
        ? tr("Screens and Control Sessions Allowed:")
        : tr("Archive Streams Allowed:");
    ui->featureNameLabel->setText(feature);
    ui->featureValueLabel->setText(QString::number(license->cameraCount()));

    QString licenseText = licenseDescription(license);

    QPushButton* copyButton = new QPushButton(this);
    copyButton->setText(tr("Copy to Clipboard"));
    connect(copyButton, &QPushButton::clicked, this, [this, licenseText] {
        qApp->clipboard()->setText(licenseText);
    });

    ui->buttonBox->addButton(copyButton, QDialogButtonBox::HelpRole);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}

QnLicenseDetailsDialog::~QnLicenseDetailsDialog() {
}

QString QnLicenseDetailsDialog::licenseDescription(const QnLicensePtr &license) const {
    QString features = (license->type() == Qn::LC_VideoWall)
        ? tr("Screens and Control Sessions Allowed: %1").arg(license->cameraCount())
        : tr("Archive Streams Allowed: %1").arg(license->cameraCount());
    QString details = tr("Generic:\n"
        "License Type: %1\n"
        "License Key: %2\n"
        "Locked to Hardware ID: %3\n"
        "\n"
        "Features:\n")
                .arg(license->displayName())
                .arg(QLatin1String(license->key()))
                .arg(QLatin1String(license->hardwareId()))
                ;
     return details + features;
}

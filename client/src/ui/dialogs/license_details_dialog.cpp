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
    ui->licenseHwidLabel->setText(QLatin1String(license->hardwareId()));

    auto addFeature = [this](const QString &text, int count) {
        QLabel* valueLabel = new QLabel(this);
        valueLabel->setText(QString::number(count));
        ui->featuresLayout->addRow(text, valueLabel);
    };

    if (license->type() == Qn::LC_VideoWall) {
        addFeature(tr("Screens Allowed:"), license->cameraCount() * 2);
        addFeature(tr("Control Sessions Allowed:"), license->cameraCount());
    } else {
        addFeature(tr("Archive Streams Allowed:"), license->cameraCount());
    }

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
    QStringList result;

    auto addStringValue = [this, &result](const QString &text, const QString &value) { result << lit("%1: %2").arg(text).arg(value); };
    auto addIntValue = [this, &result](const QString &text, int value) { result << lit("%1: %2").arg(text).arg(value); };

    result << tr("Generic:");
    addStringValue(tr("License Type"), license->displayName());
    addStringValue(tr("License Key"), QString::fromUtf8(license->key()));
    addStringValue(tr("Locked to Hardware ID"), QString::fromUtf8(license->hardwareId()));
    result << QString(); //spacer
    result << tr("Features:");
    if (license->type() == Qn::LC_VideoWall) {
        addIntValue(tr("Screens Allowed:"), license->cameraCount() * 2);
        addIntValue(tr("Control Sessions Allowed:"), license->cameraCount());
    } else {
        addIntValue(tr("Archive Streams Allowed:"), license->cameraCount());
    }
    return result.join(L'\n');
}

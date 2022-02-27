// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_details_dialog.h"
#include "ui_license_details_dialog.h"

#include <QtGui/QClipboard>

#include <QtWidgets/QPushButton>

#include <licensing/license.h>
#include <nx/vms/license/validator.h>

#include <nx/vms/client/desktop/style/custom_style.h>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::license;

namespace {

auto makeAddRowFunction(QFormLayout* layout, Aligner* aligner, QStringList* details)
{
    return
        [=](const QString& name, const QString& value)
        {
            auto titleLabel = new QLabel(name + ":", layout->parentWidget());
            auto valueLabel = new QLabel(value, layout->parentWidget());
            valueLabel->setObjectName(name + " Value");
            layout->addRow(titleLabel, valueLabel);
            aligner->addWidget(titleLabel);
            details->push_back(name + ": " + value);
        };
}

} // namespace

QnLicenseDetailsDialog::QnLicenseDetailsDialog(const QnLicensePtr& license, QWidget* parent):
    base_type(parent),
    ui(new Ui::LicenseDetailsDialog())
{
    ui->setupUi(this);

    if (!NX_ASSERT(license))
        reject();

    auto aligner = new Aligner(this);
    QStringList details;

    auto addGeneric = makeAddRowFunction(ui->genericDetailsLayout, aligner, &details);
    auto addFeature = makeAddRowFunction(ui->featuresLayout, aligner, &details);

    details.push_back(tr("Generic") + ":");
    addGeneric(tr("License Type"), license->displayName());
    addGeneric(tr("License Key"), QString::fromLatin1(license->key()));
    addGeneric(tr("Locked to Hardware ID"), license->hardwareId());

    if (license->isDeactivatable())
    {
        addGeneric(tr("Deactivations Remaining"),
            QString::number(license->deactivationsCountLeft()));
    }

    details.push_back(QString()); //< Spacer.
    details.push_back(tr("Features") + ":");
    if (license->type() == Qn::LC_VideoWall)
    {
        addFeature(tr("Screens Allowed"), QString::number(license->cameraCount() * 2));
        addFeature(tr("Control Sessions Allowed"), QString::number(license->cameraCount()));
    }
    else
    {
        addFeature(tr("Archive Streams Allowed"), QString::number(license->cameraCount()));
    }

    setWarningStyle(ui->errorLabel);
    ui->errorLabel->setText(Validator::errorMessage(
        QnLicenseErrorCode::FutureLicense,
        Qn::LC_Invalid));
    ui->errorLabel->setVisible(license->type() == Qn::LC_Invalid);

    const auto licenseDescription = details.join('\n');

    auto copyButton = new ClipboardButton(ClipboardButton::StandardType::copyLong, this);
    connect(copyButton, &QPushButton::clicked, this,
        [licenseDescription] { qApp->clipboard()->setText(licenseDescription); });

    ui->buttonBox->addButton(copyButton, QDialogButtonBox::HelpRole);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setFocus();
}

QnLicenseDetailsDialog::~QnLicenseDetailsDialog()
{
}

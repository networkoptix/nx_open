// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "version_selection_dialog.h"
#include "ui_version_selection_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/common/update/tools.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

namespace {

bool checkPassword(const QString& build, const QString& password)
{
    return ini().developerMode || nx::vms::common::update::passwordForBuild(build) == password;
}

} // namespace

VersionSelectionDialog::VersionSelectionDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::VersionSelectionDialog)
{
    ui->setupUi(this);
    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Select Version"));

    ui->versionInputField->setTitle(tr("Version"));
    ui->versionInputField->setValidator(
        [](QString text)
        {
            text = text.trimmed();

            if (text.isEmpty())
                return ValidationResult::kValid;

            bool ok = false;
            text.toInt(&ok);
            if (ok) //< Accept build number as a valid version.
                return ValidationResult::kValid;

            const nx::utils::SoftwareVersion version(text);
            return (version.major() > 0 && version.build() > 0)
                ? ValidationResult::kValid
                : ValidationResult(tr("Invalid version."));
        });

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setValidator(
        [this](const QString& password)
        {
            if (ui->versionInputField->text().isEmpty())
                return ValidationResult::kValid;

            return checkPassword(QString::number(version().build()), password.trimmed())
                ? ValidationResult::kValid
                : ValidationResult(tr("The password is incorrect."));
        });

    connect(ui->versionInputField, &InputField::textChanged, this,
        [this, okButton](const QString& text)
        {
            if (!ui->passwordInputField->text().isEmpty())
                ui->passwordInputField->validate();

            okButton->setEnabled(!text.isEmpty());
        });

    okButton->setEnabled(false);

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->versionInputField,
        ui->passwordInputField });

    setResizeToContentsMode(Qt::Vertical);
}

VersionSelectionDialog::~VersionSelectionDialog()
{
}

nx::utils::SoftwareVersion VersionSelectionDialog::version() const
{
    bool ok;
    const int build = ui->versionInputField->text().toInt(&ok);
    if (ok) //< Accept build number as a valid version.
        return nx::utils::SoftwareVersion(0, 0, 0, build);

    return nx::utils::SoftwareVersion(ui->versionInputField->text().trimmed());
}

QString VersionSelectionDialog::password() const
{
    return ui->passwordInputField->text();
}

void VersionSelectionDialog::accept()
{
    if (ui->passwordInputField->validate())
        base_type::accept();
}

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeated_password_dialog.h"
#include "ui_repeated_password_dialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

RepeatedPasswordDialog::RepeatedPasswordDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::RepeatedPasswordDialog)
{
    ui->setupUi(this);
    setModal(true);
    setWarningStyle(ui->doNotMatchLabel);
    WidgetUtils::setRetainSizeWhenHidden(ui->doNotMatchLabel, true);
    updateMatching();

    connect(ui->passwordInput, &InputField::textChanged,
        this, &RepeatedPasswordDialog::updateMatching);
    connect(ui->repeatInput, &InputField::textChanged,
        this, &RepeatedPasswordDialog::updateMatching);
}

RepeatedPasswordDialog::~RepeatedPasswordDialog()
{
}

void RepeatedPasswordDialog::setHeaderText(const QString& text)
{
    ui->headerInfoBar->setText(text);
}

QString RepeatedPasswordDialog::headerText() const
{
    return ui->headerInfoBar->text();
}

QString RepeatedPasswordDialog::password() const
{
    if (!passwordsMatch())
        return QString();

    return ui->passwordInput->text();
}

void RepeatedPasswordDialog::clearInputFields()
{
    ui->passwordInput->clear();
    ui->repeatInput->clear();
    ui->passwordInput->setFocus();
}

bool RepeatedPasswordDialog::passwordsMatch() const
{
    return ui->passwordInput->text() == ui->repeatInput->text();
}

void RepeatedPasswordDialog::updateMatching()
{
    const bool match = passwordsMatch();
    const bool passwordIsEmpty = ui->passwordInput->text().isEmpty();
    const bool warning = !match && !passwordIsEmpty && !ui->repeatInput->text().isEmpty();
    ui->doNotMatchLabel->setVisible(warning);
    setWarningStyleOn(ui->passwordInput, warning);
    setWarningStyleOn(ui->repeatInput, warning);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(match && !passwordIsEmpty);
}

} // namespace nx::vms::client::desktop

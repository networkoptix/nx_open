// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "input_dialog.h"
#include "ui_input_dialog.h"

QnInputDialog::QnInputDialog(QWidget* parent): base_type(parent), ui(new Ui::InputDialog())
{
    ui->setupUi(this);

    ui->valueLineEdit->setValidator(
        nx::vms::client::desktop::defaultNonEmptyValidator(tr("Please enter a value.")));
    ui->passwordLineEdit->setValidator(
        nx::vms::client::desktop::defaultNonEmptyValidator(tr("Please enter a password.")));

    setResizeToContentsMode(Qt::Horizontal | Qt::Vertical);
    ensureUsedForPassword();
}

QnInputDialog::~QnInputDialog()
{
}

QString QnInputDialog::caption() const
{
    return ui->captionLabel->text();
}

void QnInputDialog::setCaption(const QString& caption)
{
    ui->captionLabel->setText(caption);
}

QString QnInputDialog::value() const
{
    return usedForPassword
        ? ui->passwordLineEdit->text().trimmed()
        : ui->valueLineEdit->text().trimmed();
}

void QnInputDialog::setValue(const QString& value)
{
    ui->valueLineEdit->setText(value);
}

void QnInputDialog::setValidator(nx::vms::client::desktop::TextValidateFunction validator)
{
    ui->valueLineEdit->setValidator(validator);
    ui->passwordLineEdit->setValidator(validator);
}

QString QnInputDialog::placeholderText() const
{
    return ui->valueLineEdit->placeholderText();
}

void QnInputDialog::setPlaceholderText(const QString& placeholderText)
{
    ui->valueLineEdit->setPlaceholderText(placeholderText);
    ui->passwordLineEdit->setPlaceholderText(placeholderText);
}

QDialogButtonBox::StandardButtons QnInputDialog::buttons() const
{
    return ui->buttonBox->standardButtons();
}

void QnInputDialog::setButtons(QDialogButtonBox::StandardButtons buttons)
{
    ui->buttonBox->setStandardButtons(buttons);
}

void QnInputDialog::useForPassword()
{
    usedForPassword = true;
    ensureUsedForPassword();
}

QString QnInputDialog::getText(QWidget* parent,
    const QString& title,
    const QString& label,
    QDialogButtonBox::StandardButtons buttons,
    const QString& placeholder,
    const QString& initialText)
{
    QnInputDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setCaption(label);
    dialog.setButtons(buttons);
    dialog.setPlaceholderText(placeholder);
    dialog.setValue(initialText);

    if (dialog.exec() != Accepted)
        return QString();

    return dialog.value();
}

void QnInputDialog::accept()
{
    auto lineEdit = usedForPassword ? ui->passwordLineEdit : ui->valueLineEdit;
    if (!lineEdit->validate())
        return;

    base_type::accept();
}

void QnInputDialog::ensureUsedForPassword()
{
    ui->passwordLineEdit->setVisible(usedForPassword);
    ui->valueLineEdit->setVisible(!usedForPassword);
}

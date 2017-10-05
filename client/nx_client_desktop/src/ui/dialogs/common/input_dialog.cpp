#include "input_dialog.h"
#include "ui_input_dialog.h"

#include <ui/style/custom_style.h>

QnInputDialog::QnInputDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::InputDialog())
{
    ui->setupUi(this);

    setWarningStyle(ui->errorLabel);

    connect(ui->valueLineEdit, &QLineEdit::textChanged, this, &QnInputDialog::validateInput);
    validateInput();

    setResizeToContentsMode(Qt::Horizontal | Qt::Vertical);
}

QnInputDialog::~QnInputDialog()
{}

QString QnInputDialog::caption() const
{
    return ui->captionLabel->text();
}

void QnInputDialog::setCaption(const QString &caption)
{
    ui->captionLabel->setText(caption);
}

QString QnInputDialog::value() const
{
    return ui->valueLineEdit->text().trimmed();
}

void QnInputDialog::setValue(const QString &value)
{
    ui->valueLineEdit->setText(value);
    validateInput();
}

QString QnInputDialog::placeholderText() const
{
    return ui->valueLineEdit->placeholderText();
}

void QnInputDialog::setPlaceholderText(const QString &placeholderText)
{
    ui->valueLineEdit->setPlaceholderText(placeholderText);
}

QLineEdit::EchoMode QnInputDialog::echoMode() const
{
    return ui->valueLineEdit->echoMode();
}

void QnInputDialog::setEchoMode(QLineEdit::EchoMode echoMode)
{
    ui->valueLineEdit->setEchoMode(echoMode);
}

QDialogButtonBox::StandardButtons QnInputDialog::buttons() const
{
    return ui->buttonBox->standardButtons();
}

void QnInputDialog::setButtons(QDialogButtonBox::StandardButtons buttons)
{
    ui->buttonBox->setStandardButtons(buttons);
    validateInput();
}

void QnInputDialog::validateInput()
{
    bool valid = !value().isEmpty();
    for(QAbstractButton *button: ui->buttonBox->buttons())
    {
        QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(button);

        if(role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole || role == QDialogButtonBox::ApplyRole)
            button->setEnabled(valid);
    }

    ui->errorLabel->setVisible(false);
}

QString QnInputDialog::getText(QWidget* parent,
    const QString& title, const QString& label,
    QDialogButtonBox::StandardButtons buttons,
    QLineEdit::EchoMode echoMode,
    const QString& placeholder,
    const QString& initialText)
{
    QnInputDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setCaption(label);
    dialog.setButtons(buttons);
    dialog.setPlaceholderText(placeholder);
    dialog.setEchoMode(echoMode);
    dialog.setValue(initialText);

    if (dialog.exec() != Accepted)
        return QString();

    return dialog.value();
}

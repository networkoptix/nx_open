#include "input_dialog.h"
#include "ui_input_dialog.h"

QnInputDialog::QnInputDialog(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::InputDialog())
{
    ui->setupUi(this);

    ui->valueLineEdit->setValidator(nx::vms::client::desktop::defaultNonEmptyValidator(tr("Please enter a value.")));

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
}

void QnInputDialog::setValidator(nx::vms::client::desktop::TextValidateFunction validator)
{
    ui->valueLineEdit->setValidator(validator);
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
}

void QnInputDialog::useForPassword()
{
    ui->valueLineEdit->useForPassword();
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

void QnInputDialog::accept()
{
    if (!ui->valueLineEdit->validate())
        return;

    base_type::accept();
}

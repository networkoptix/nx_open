#include "layout_name_dialog.h"
#include "ui_layout_name_dialog.h"

#include <QtWidgets/QPushButton>

QnLayoutNameDialog::QnLayoutNameDialog(const QString &caption, const QString &text, const QString &name, QDialogButtonBox::StandardButtons buttons, QWidget *parent):
    base_type(parent)
{
    init(buttons);

    setText(text);
    setName(name);
    setWindowTitle(caption);
}

QnLayoutNameDialog::QnLayoutNameDialog(QDialogButtonBox::StandardButtons buttons, QWidget *parent):
    base_type(parent)
{
    init(buttons);
}

void QnLayoutNameDialog::init(QDialogButtonBox::StandardButtons buttons) {
    ui.reset(new Ui::LayoutNameDialog());

    ui->setupUi(this);

    ui->buttonBox->setStandardButtons(buttons);

    connect(ui->nameLineEdit,   SIGNAL(textChanged(const QString &)),   this,   SLOT(at_nameLineEdit_textChanged(const QString &)));
    at_nameLineEdit_textChanged(ui->nameLineEdit->text());
}

QnLayoutNameDialog::~QnLayoutNameDialog() {
    return;
}

QString QnLayoutNameDialog::name() const {
    return ui->nameLineEdit->text().trimmed();
}

void QnLayoutNameDialog::setName(const QString &name) {
    ui->nameLineEdit->setText(name);
}

QString QnLayoutNameDialog::text() const {
    return ui->label->text();
}

void QnLayoutNameDialog::setText(const QString &text) {
    ui->label->setText(text);
}

void QnLayoutNameDialog::at_nameLineEdit_textChanged(const QString &text) {
    foreach(QAbstractButton *button, ui->buttonBox->buttons()) {
        QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(button);
        
        if(role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole || role == QDialogButtonBox::ApplyRole)
            button->setEnabled(!text.isEmpty());
    }
}

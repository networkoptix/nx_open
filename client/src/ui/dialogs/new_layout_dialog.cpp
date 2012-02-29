#include "new_layout_dialog.h"
#include <QPushButton>
#include "ui_new_layout_dialog.h"

QnNewLayoutDialog::QnNewLayoutDialog(QWidget *parent):
    QDialog(parent),
    ui(new Ui::NewLayoutDialog)
{
    ui->setupUi(this);

    at_nameLineEdit_textChanged(ui->nameLineEdit->text());
}

QnNewLayoutDialog::~QnNewLayoutDialog() {
    return;
}

QString QnNewLayoutDialog::name() {
    return ui->nameLineEdit->text();
}

void QnNewLayoutDialog::setName(const QString &name) {
    ui->nameLineEdit->setText(name);
}

void QnNewLayoutDialog::at_nameLineEdit_textChanged(const QString& text) {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
}

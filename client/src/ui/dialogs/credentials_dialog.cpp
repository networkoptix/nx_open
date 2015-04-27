#include "credentials_dialog.h"
#include "ui_credentials_dialog.h"

QnCredentialsDialog::QnCredentialsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnCredentialsDialog)
{
    ui->setupUi(this);
}

QnCredentialsDialog::~QnCredentialsDialog() {}

QString QnCredentialsDialog::user() const {
    return ui->userEdit->text();
}

void QnCredentialsDialog::setUser(const QString &user) {
    ui->userEdit->setText(user);
}

QString QnCredentialsDialog::password() const {
    return ui->passwordEdit->text();
}

void QnCredentialsDialog::setPassword(const QString &password) {
    ui->passwordEdit->setText(password);
}

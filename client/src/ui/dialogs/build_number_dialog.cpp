#include "build_number_dialog.h"
#include "ui_build_number_dialog.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "utils/update/update_utils.h"

namespace {
    bool checkPassword(int buildNumber, const QString &password) {
        return passwordForBuild((unsigned)buildNumber) == password;
    }
}

QnBuildNumberDialog::QnBuildNumberDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QnBuildNumberDialog)
{
    ui->setupUi(this);
    ui->buildNumberEdit->setValidator(new QIntValidator(ui->buildNumberEdit));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Update"));
}

QnBuildNumberDialog::~QnBuildNumberDialog() {}

int QnBuildNumberDialog::buildNumber() const {
    return ui->buildNumberEdit->text().toInt();
}

QString QnBuildNumberDialog::password() const {
    return ui->passwordEdit->text();
}

void QnBuildNumberDialog::accept() {
    if (checkPassword(buildNumber(), password())) {
        base_type::accept();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("The password you have entered is invalid"));
    }
}

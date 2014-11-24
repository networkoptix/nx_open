#include "build_number_dialog.h"
#include "ui_build_number_dialog.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "utils/update/update_utils.h"

namespace {
    bool checkPassword(int buildNumber, const QString &password) {
#ifdef _DEBUG
        Q_UNUSED(buildNumber);
        Q_UNUSED(password);
        return true;
#else
        return passwordForBuild((unsigned)buildNumber) == password;
#endif
    }
}

QnBuildNumberDialog::QnBuildNumberDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnBuildNumberDialog)
{
    ui->setupUi(this);
    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    connect(ui->buildNumberEdit, &QLineEdit::textChanged, this, [this, okButton](const QString &text) {
        okButton->setEnabled(text.toInt() > 0);
    });
    okButton->setEnabled(false);
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

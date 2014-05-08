#include "build_number_dialog.h"
#include "ui_build_number_dialog.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

namespace {
    const char passwordChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    const int passwordLength = 6;

    QString getPasswordForBuild(unsigned buildNumber) {
        unsigned seed1 = buildNumber;
        unsigned seed2 = (buildNumber + 13) * 179;
        unsigned seed3 = buildNumber << 16;
        unsigned seed4 = ((buildNumber + 179) * 13) << 16;
        unsigned seed = seed1 ^ seed2 ^ seed3 ^ seed4;

        QString password;
        const int charsCount = sizeof(passwordChars) - 1;
        for (int i = 0; i < passwordLength; i++) {
            password += QChar::fromLatin1(passwordChars[seed % charsCount]);
            seed /= charsCount;
        }
        return password;
    }

    bool checkPassword(int buildNumber, const QString &password) {
        return getPasswordForBuild((unsigned)buildNumber) == password;
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

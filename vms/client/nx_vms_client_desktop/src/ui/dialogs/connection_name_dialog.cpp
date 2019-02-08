#include "connection_name_dialog.h"
#include "ui_connection_name_dialog.h"

#include <QtWidgets/QPushButton>

QnConnectionNameDialog::QnConnectionNameDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::ConnectionNameDialog())
{
    ui->setupUi(this);

    QPushButton* okButton = ui->buttonBox->button(QDialogButtonBox::Ok);

    auto validate = [this, okButton](const QString &text) {
        okButton->setEnabled(!text.trimmed().isEmpty());
    };

    connect(ui->nameLineEdit,   &QLineEdit::textChanged,   this,   validate);
    okButton->setEnabled(false);
}

QnConnectionNameDialog::~QnConnectionNameDialog() {
}

QString QnConnectionNameDialog::name() const {
    return ui->nameLineEdit->text().trimmed();
}

void QnConnectionNameDialog::setName(const QString &name) {
    ui->nameLineEdit->setText(name);
}

void QnConnectionNameDialog::at_nameLineEdit_textChanged(const QString &text) {
    foreach(QAbstractButton *button, ui->buttonBox->buttons()) {
        QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(button);
        
        if(role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole || role == QDialogButtonBox::ApplyRole)
            button->setEnabled(!text.isEmpty());
    }
}

bool QnConnectionNameDialog::savePassword() const {
    return ui->savePasswordCheckBox->isChecked();
}

void QnConnectionNameDialog::setSavePassword(bool savePassword) {
    ui->savePasswordCheckBox->setCheckState(savePassword ? Qt::Checked : Qt::Unchecked);
}

void QnConnectionNameDialog::setSavePasswordEnabled(bool enabled) {
    ui->savePasswordCheckBox->setEnabled(enabled);
}

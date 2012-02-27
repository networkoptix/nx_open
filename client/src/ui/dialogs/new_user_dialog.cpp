#include "new_user_dialog.h"
#include <QPushButton>
#include "ui_new_user_dialog.h"

namespace {
    QColor redText = QColor(255, 64, 64);
}

QnNewUserDialog::QnNewUserDialog(QWidget *parent): 
    QDialog(parent),
    ui(new Ui::NewUserDialog())
{
    memset(m_valid, sizeof(m_valid), 0);

    ui->setupUi(this);

    ui->accessRightsComboBox->addItem(tr("Viewer"), false);
    ui->accessRightsComboBox->addItem(tr("Administrator"), true);

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateLogin()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->confirmPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->accessRightsComboBox,   SIGNAL(currentIndexChanged(int)),       this,   SLOT(updatePassword()));

    {
        QPalette palette = ui->hintLabel->palette();
        palette.setColor(QPalette::WindowText, redText);
        ui->hintLabel->setPalette(palette);
    }

    updateAll();
}

QnNewUserDialog::~QnNewUserDialog() {
    return;
}

void QnNewUserDialog::setHint(Element element, const QString &hint) {
    if(m_hints[element] == hint)
        return;

    m_hints[element] = hint;

    int i;
    for(i = 0; i < ElementCount; i++)
        if(!m_hints[i].isEmpty())
            break;

    QString actualHint = i == ElementCount ? QString() : m_hints[i];
    ui->hintLabel->setText(actualHint);
}

void QnNewUserDialog::setValid(Element element, bool valid) {
    if(m_valid[element] == valid)
        return;

    m_valid[element] = valid;

    QPalette palette = this->palette();
    if(!valid)
        palette.setColor(QPalette::WindowText, redText);

    switch(element) {
    case Login:
        ui->loginLabel->setPalette(palette);
        break;
    case Password:
        ui->passwordLabel->setPalette(palette);
        ui->confirmPasswordLabel->setPalette(palette);
        break;
    case AccessRights:
        ui->accessRightsLabel->setPalette(palette);
        break;
    default:
        break;
    }

    bool allValid = m_valid[Login] && m_valid[Password] && m_valid[AccessRights];
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(allValid);
}

void QnNewUserDialog::updateElement(Element element) {
    bool valid = true;
    QString hint;

    switch(element) {
    case Login:
        if(ui->loginEdit->text().isEmpty()) {
            hint = tr("Login can not be empty.");
            valid = false;
        }
        break;
    case Password:
        if(ui->passwordEdit->text().isEmpty() && ui->confirmPasswordEdit->text().isEmpty()) {
            hint = tr("Password can not be empty.");
            valid = false;
        } else if(ui->passwordEdit->text() != ui->confirmPasswordEdit->text()) {
            hint = tr("Passwords do not match.");
            valid = false;
        }
        break;
    case AccessRights:
        if(ui->accessRightsComboBox->currentIndex() == -1) {
            hint = tr("Choose access rights.");
            valid = false;
        }
        break;
    default:
        break;
    }

    setValid(element, valid);
    setHint(element, hint);
}

void QnNewUserDialog::updateAll() {
    updateElement(AccessRights);
    updateElement(Password);
    updateElement(Login);
}

QString QnNewUserDialog::login() const {
    return ui->loginEdit->text();
}

QString QnNewUserDialog::password() const {
    return ui->passwordEdit->text();
}

bool QnNewUserDialog::isAdmin() const {
    return ui->accessRightsComboBox->itemData(ui->accessRightsComboBox->currentIndex()).toBool();
}

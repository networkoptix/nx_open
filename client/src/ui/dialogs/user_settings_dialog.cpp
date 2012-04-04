#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <QtGui/QPushButton>
#include <QtGui/QKeyEvent>

#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <utils/common/event_processors.h>
#include <utils/common/warnings.h>

#include <ui/common/read_only.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

QnUserSettingsDialog::QnUserSettingsDialog(QnWorkbenchContext *context, QWidget *parent): 
    QDialog(parent),
    ui(new Ui::UserSettingsDialog()),
    m_context(context),
    m_hasChanges(false)
{
    if(context == NULL) 
        qnNullWarning(context);

    foreach(const QnResourcePtr &user, context->resourcePool()->getResourcesWithFlag(QnResource::user))
        m_userByLogin[user->getName()] = user;

    for(int i = 0; i < ElementCount; i++) {
        m_valid[i] = false;
        m_flags[i] = Editable | Visible;
    }

    ui->setupUi(this);

    ui->accessRightsComboBox->addItem(tr("Viewer"), false);
    ui->accessRightsComboBox->addItem(tr("Administrator"), true); 

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateLogin()));
    connect(ui->currentPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updateCurrentPassword()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updateCurrentPassword()));
    connect(ui->confirmPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->confirmPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updateCurrentPassword()));
    connect(ui->accessRightsComboBox,   SIGNAL(currentIndexChanged(int)),       this,   SLOT(updateAccessRights()));

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(setHasChanges()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(setHasChanges()));
    connect(ui->confirmPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(setHasChanges()));
    connect(ui->accessRightsComboBox,   SIGNAL(currentIndexChanged(int)),       this,   SLOT(setHasChanges()));

    {
        QPalette palette = ui->hintLabel->palette();
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
        ui->hintLabel->setPalette(palette);
    }

    updateAll();
}

QnUserSettingsDialog::~QnUserSettingsDialog() {
    return;
}

void QnUserSettingsDialog::setHasChanges(bool hasChanges) {
    if(m_hasChanges == hasChanges)
        return;

    m_hasChanges = hasChanges;

    emit hasChangesChanged();
}

void QnUserSettingsDialog::setElementFlags(Element element, ElementFlags flags) {
    if(element < 0 || element >= ElementCount) {
        qnWarning("Invalid element '%1'.", static_cast<int>(element));
        return;
    }
    
    m_flags[element] = flags;

    bool visible = flags & Visible;
    bool editable = flags & Editable;

    switch(element) {
    case Login:
        ui->loginEdit->setVisible(visible);
        ui->loginLabel->setVisible(visible);
        ui->loginEdit->setReadOnly(!editable);
        break;
    case CurrentPassword:
        ui->currentPasswordEdit->setVisible(visible);
        ui->currentPasswordLabel->setVisible(visible);
        ui->currentPasswordEdit->setReadOnly(!editable);
        if(visible) {
            ui->passwordLabel->setText(tr("New Password"));
        } else {
            ui->passwordLabel->setText(tr("Password"));
        }
        break;
    case Password:
        ui->passwordEdit->setVisible(visible);
        ui->passwordLabel->setVisible(visible);
        ui->confirmPasswordEdit->setVisible(visible);
        ui->confirmPasswordLabel->setVisible(visible);
        ui->passwordEdit->setReadOnly(!editable);
        ui->confirmPasswordEdit->setReadOnly(!editable);
        break;
    case AccessRights:
        ui->accessRightsLabel->setVisible(visible);
        ui->accessRightsComboBox->setVisible(visible);
        setReadOnly(ui->accessRightsComboBox, !editable);
        break;
    default:
        break;
    }

    updateElement(element);
}

QnUserSettingsDialog::ElementFlags QnUserSettingsDialog::elementFlags(Element element) const {
    if(element < 0 || element >= ElementCount) {
        qnWarning("Invalid element '%1'.", static_cast<int>(element));
        return 0;
    }

    return m_flags[element];
}

const QnUserResourcePtr &QnUserSettingsDialog::user() const {
    return m_user;
}

void QnUserSettingsDialog::setUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    m_user = user;

    updateFromResource();
}

const QString &QnUserSettingsDialog::currentPassword() const {
    return m_currentPassword;
}

void QnUserSettingsDialog::setCurrentPassword(const QString &currentPassword) {
    m_currentPassword = currentPassword;

    updateElement(CurrentPassword);
}

void QnUserSettingsDialog::updateFromResource() {
    if(!m_user) {
        ui->loginEdit->clear();
        ui->currentPasswordEdit->clear();
        ui->passwordEdit->clear();
        ui->passwordEdit->setPlaceholderText(QString());
        ui->confirmPasswordEdit->clear();
        ui->confirmPasswordEdit->setPlaceholderText(QString());
        ui->accessRightsComboBox->setCurrentIndex(-1);
    } else {
        QString placeholder(m_user->getPassword().size(), QChar('*'));

        ui->loginEdit->setText(m_user->getName());
        ui->currentPasswordEdit->clear();
        ui->passwordEdit->setPlaceholderText(placeholder);
        ui->passwordEdit->clear();
        ui->confirmPasswordEdit->setPlaceholderText(placeholder);
        ui->confirmPasswordEdit->clear();
        ui->accessRightsComboBox->setCurrentIndex(m_user->isAdmin() ? 1 : 0); // TODO: evil magic numbers

        updatePassword();
    }

    setHasChanges(false);
}

void QnUserSettingsDialog::submitToResource() {
    if(!m_user)
        return;

    m_user->setName(ui->loginEdit->text());
    if(!ui->passwordEdit->text().isEmpty())
        m_user->setPassword(ui->passwordEdit->text());
    m_user->setAdmin(ui->accessRightsComboBox->itemData(ui->accessRightsComboBox->currentIndex()).toBool());

    setHasChanges(false);
}

void QnUserSettingsDialog::setHint(Element element, const QString &hint) {
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

void QnUserSettingsDialog::setValid(Element element, bool valid) {
    if(m_valid[element] == valid)
        return;

    m_valid[element] = valid;

    QPalette palette = this->palette();
    if(!valid)
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());

    switch(element) {
    case Login:
        ui->loginLabel->setPalette(palette);
        break;
    case Password:
        ui->passwordLabel->setPalette(palette);
        ui->confirmPasswordLabel->setPalette(palette);
        break;
    case CurrentPassword:
        ui->currentPasswordLabel->setPalette(palette);
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

void QnUserSettingsDialog::updateElement(Element element) {
    bool valid = true;
    QString hint;

    switch(element) {
    case Login:
        if(ui->loginEdit->text().isEmpty()) {
            hint = tr("Login cannot be empty.");
            valid = false;
        }
        if(m_userByLogin.contains(ui->loginEdit->text()) && m_userByLogin.value(ui->loginEdit->text()) != m_user) {
            hint = tr("User with specified login already exists.");
            valid = false;
        }
        break;
    case CurrentPassword:
        if((!ui->passwordEdit->text().isEmpty() || !ui->confirmPasswordEdit->text().isEmpty()) && !m_currentPassword.isEmpty() && ui->currentPasswordEdit->text() != m_currentPassword) {
            if(ui->currentPasswordEdit->text().isEmpty()) {
                hint = tr("To change your password, please enter your current password.");
                valid = false;
            } else {
                hint = tr("Invalid current password.");
                valid = false;
            }
        }
        break;
    case Password:
        if(ui->passwordEdit->text().isEmpty() && ui->confirmPasswordEdit->text().isEmpty() && ui->passwordEdit->placeholderText().isEmpty()) {
            hint = tr("Password cannot be empty.");
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

    if(m_flags[element] != (Visible | Editable)) {
        valid = true;
        hint = QString();
    }

    setValid(element, valid);
    setHint(element, hint);
}

void QnUserSettingsDialog::updateAll() {
    updateElement(AccessRights);
    updateElement(Password);
    updateElement(CurrentPassword);
    updateElement(Login);
}

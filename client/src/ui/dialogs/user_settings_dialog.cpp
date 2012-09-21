#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <QtGui/QPushButton>
#include <QtGui/QCheckBox>
#include <QtGui/QKeyEvent>

#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>
#include <utils/common/event_processors.h>
#include <utils/common/warnings.h>

#include <ui/common/read_only.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_globals.h>

#define CUSTOM_RIGHTS 0x0FFFFFFF

QnUserSettingsDialog::QnUserSettingsDialog(QnWorkbenchContext *context, QWidget *parent): 
    QDialog(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::UserSettingsDialog()),
    m_user(0),
    m_hasChanges(false),
    m_editorRights(0)
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

    ui->accessRightsGroupbox->hide();

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

    //connect(ui->advancedButton,         SIGNAL(toggled(bool)),                  ui->accessRightsGroupbox,   SLOT(setVisible(bool)));
    connect(ui->advancedButton,         SIGNAL(toggled(bool)),                  this,   SLOT(at_advancedButton_toggled()));

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
        ui->advancedButton->setVisible(visible);
//        ui->accessRightsGroupbox->setEnabled(editable);
        setReadOnly(ui->accessRightsComboBox, !editable);
        setReadOnly(ui->accessRightsGroupbox, !editable);
        // TODO: #gdm if readonly then do not save anyway
        break;
    default:
        break;
    }

    updateElement(element);
}

void QnUserSettingsDialog::setEditorPermissions(quint64 rights) {
    m_editorRights = rights;
    if (m_user) {
        createAccessRightsPresets();
        createAccessRightsAdvanced();
    }
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
    if (m_editorRights){
        createAccessRightsPresets();
        createAccessRightsAdvanced();
    }

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
        QString placeholder(m_user->getPassword().size(), QLatin1Char('*'));

        ui->loginEdit->setText(m_user->getName());
        ui->currentPasswordEdit->clear();
        ui->passwordEdit->setPlaceholderText(placeholder);
        ui->passwordEdit->clear();
        ui->confirmPasswordEdit->setPlaceholderText(placeholder);
        ui->confirmPasswordEdit->clear();

        loadAccessRightsToUi(context()->accessController()->calculateGlobalPermissions(m_user));
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

    quint64 rights = ui->accessRightsComboBox->itemData(ui->accessRightsComboBox->currentIndex()).toULongLong();
    if (rights == CUSTOM_RIGHTS)
        rights = readAccessRightsAdvanced();

    m_user->setPermissions(rights);

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
        } else {
            quint64 rights = ui->accessRightsComboBox->itemData(ui->accessRightsComboBox->currentIndex()).toULongLong();
            if (rights == CUSTOM_RIGHTS)
                ui->advancedButton->setChecked(true);
            else
                fillAccessRightsAdvanced(rights);
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

void QnUserSettingsDialog::loadAccessRightsToUi(quint64 rights) {
    selectAccessRightsPreset(rights);
    fillAccessRightsAdvanced(rights);
}

void QnUserSettingsDialog::updateAll() {
    updateElement(AccessRights);
    updateElement(Password);
    updateElement(CurrentPassword);
    updateElement(Login);
}

void QnUserSettingsDialog::at_accessRights_changed() {
    setHasChanges(true);
    selectAccessRightsPreset(readAccessRightsAdvanced());
}


void QnUserSettingsDialog::createAccessRightsPresets() {
    if (!m_user)
        return;

    Qn::Permissions permissions = context()->accessController()->calculateGlobalPermissions(m_user);

    // show only for view of owner
    if (permissions & Qn::GlobalEditProtectedUserPermission)
        ui->accessRightsComboBox->addItem(tr("Owner"), (quint64)Qn::GlobalOwnerPermission);

    // show for an admin or for anyone opened by owner
    if ((permissions & Qn::GlobalProtectedPermission) || (m_editorRights & Qn::GlobalEditProtectedUserPermission))
        ui->accessRightsComboBox->addItem(tr("Administrator"), (quint64)Qn::GlobalAdminPermission);

    ui->accessRightsComboBox->addItem(tr("Advanced Viewer"), (quint64)Qn::GlobalAdvancedViewerPermission);
    ui->accessRightsComboBox->addItem(tr("Viewer"), (quint64)Qn::GlobalViewerPermission);
    ui->accessRightsComboBox->addItem(tr("Live Viewer"), (quint64)Qn::GlobalLiveViewerPermission);

    ui->accessRightsComboBox->addItem(tr("Custom..."), (quint64)CUSTOM_RIGHTS); // should be the last
}

void QnUserSettingsDialog::createAccessRightsAdvanced() {
    if (!m_user)
        return;

    Qn::Permissions permissions = context()->accessController()->calculateGlobalPermissions(m_user);
    QWidget* previous = ui->advancedButton;

    if (permissions & Qn::GlobalEditProtectedUserPermission)
        previous = createAccessRightCheckBox(tr("Owner"), Qn::GlobalEditProtectedUserPermission, previous);
    if ((permissions & Qn::GlobalProtectedPermission) || (m_editorRights & Qn::GlobalEditProtectedUserPermission))
        previous = createAccessRightCheckBox(tr("Administrator"),
                     Qn::GlobalProtectedPermission | Qn::GlobalEditUsersPermission | Qn::GlobalEditLayoutsPermission | Qn::GlobalEditServersPermissions,
                     previous);
    previous = createAccessRightCheckBox(tr("Can adjust camera settings"), Qn::GlobalEditCamerasPermission, previous);
    createAccessRightCheckBox(tr("Can view video archives"), Qn::GlobalViewArchivePermission, previous);
}

QCheckBox *QnUserSettingsDialog::createAccessRightCheckBox(QString text, quint64 right, QWidget *previous) {
    QCheckBox *checkBox = new QCheckBox(text, this);
    ui->accessRightsGroupbox->layout()->addWidget(checkBox);
    m_advancedRights.insert(right, checkBox);
    setTabOrder(previous, checkBox);

    if (isReadOnly(ui->accessRightsGroupbox))
        setReadOnly(checkBox, true);
    else
        connect(checkBox, SIGNAL(clicked()), this, SLOT(at_accessRights_changed()));
    return checkBox;
}

void QnUserSettingsDialog::selectAccessRightsPreset(quint64 rights) {
    bool custom = true;
    for (int i = 0; i < ui->accessRightsComboBox->count(); i++){
        if (ui->accessRightsComboBox->itemData(i).toULongLong() == rights) {
            ui->accessRightsComboBox->setCurrentIndex(i);
            custom = false;
            break;
        }
    }

    if (custom){
        ui->advancedButton->setChecked(true);
        ui->accessRightsComboBox->setCurrentIndex(ui->accessRightsComboBox->count() - 1);
    }
}

void QnUserSettingsDialog::fillAccessRightsAdvanced(quint64 rights) {
    QHashIterator<quint64, QCheckBox*> i(m_advancedRights);
    while (i.hasNext()) {
        i.next();
        i.value()->setChecked(i.key() & rights);
    }
}

quint64 QnUserSettingsDialog::readAccessRightsAdvanced() {
    quint64 rights = Qn::GlobalViewLivePermission;
    QHashIterator<quint64, QCheckBox*> i(m_advancedRights);
    while (i.hasNext()) {
        i.next();
        if (i.value()->isChecked())
            rights |= i.key();
    }
    return rights;
}

void QnUserSettingsDialog::at_advancedButton_toggled() {
    ui->accessRightsGroupbox->setVisible(ui->advancedButton->isChecked());

    /* Do forced activation of all layouts in the chain to avoid flicker.
     * Note that this is an awful hack. It would be nice to find a better solution. */
    QWidget *widget = ui->accessRightsGroupbox;
    while(widget) {
        if(widget->layout()) {
            widget->layout()->update();
            widget->layout()->activate();
        }
        
        widget = widget->parentWidget();
    }
}

#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtGui/QKeyEvent>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/event_processors.h>
#include <utils/common/warnings.h>
#include <utils/common/email.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/warning_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <client/client_globals.h>

#define CUSTOM_RIGHTS (quint64)0x0FFFFFFF

namespace Qn {
    const quint64 ExcludingOwnerPermission = GlobalOwnerPermissions & ~GlobalAdminPermissions;
    const quint64 ExcludingAdminPermission = GlobalAdminPermissions & ~GlobalAdvancedViewerPermissions;
}

QnUserSettingsDialog::QnUserSettingsDialog(QnWorkbenchContext *context, QWidget *parent): 
    base_type(parent),
    ui(new Ui::UserSettingsDialog()),
    m_user(0),
    m_hasChanges(false),
    m_inUpdateDependensies(false),
    m_userNameModified(false),
    m_passwordModified(false),
    m_emailModified(false),
    m_accessRightsModified(false)
{
    if(context == NULL) 
        qnNullWarning(context);

    foreach(const QnResourcePtr &user, context->resourcePool()->getResourcesWithFlag(Qn::user))
        m_userByLogin[user->getName().toLower()] = user;

    std::fill(m_valid.begin(), m_valid.end(), true);
    std::fill(m_flags.begin(), m_flags.end(), Editable | Visible);
    
    ui->setupUi(this);

    setHelpTopic(ui->accessRightsLabel, ui->accessRightsComboBox,   Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->accessRightsGroupbox,                          Qn::UserSettings_UserRoles_Help);

    ui->accessRightsGroupbox->hide();

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateLogin()));
    connect(ui->currentPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->confirmPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->emailEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateEmail()));
    connect(ui->accessRightsComboBox,   SIGNAL(currentIndexChanged(int)),       this,   SLOT(updateAccessRights()));

    connect(ui->loginEdit,              &QLineEdit::textChanged, this,   [this](){ setHasChanges(); m_userNameModified = true; } );
    connect(ui->passwordEdit,           &QLineEdit::textChanged, this,   [this](){ setHasChanges(); m_passwordModified = true; } );
    connect(ui->confirmPasswordEdit,    &QLineEdit::textChanged, this,   [this](){ setHasChanges(); } );
    connect(ui->emailEdit,              &QLineEdit::textChanged, this,   [this](){ setHasChanges(); m_emailModified = true; } );
    connect(ui->accessRightsComboBox,   static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this,   [this](){ setHasChanges(); m_accessRightsModified = true; } );

    //connect(ui->advancedButton,         SIGNAL(toggled(bool)),                  ui->accessRightsGroupbox,   SLOT(setVisible(bool)));
    connect(ui->advancedButton,         SIGNAL(toggled(bool)),                  this,   SLOT(at_advancedButton_toggled()));

    setWarningStyle(ui->hintLabel);

    updateAll();
    updateSizeLimits();
}

QnUserSettingsDialog::~QnUserSettingsDialog() {
    return;
}

void QnUserSettingsDialog::setHasChanges(bool hasChanges) {
    if(m_hasChanges == hasChanges)
        return;

    m_hasChanges = hasChanges;
    if( !hasChanges )
    {
        m_userNameModified = false;
        m_passwordModified = false;
        m_emailModified = false;
        m_accessRightsModified = false;
    }

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
        // TODO: #GDM #Common if readonly then do not save anyway
        break;
    case Email:
        ui->emailEdit->setVisible(visible);
        ui->emailLabel->setVisible(visible);
        ui->emailEdit->setReadOnly(!editable);
        break;
    default:
        break;
    }

    updateElement(element);
}

void QnUserSettingsDialog::setFocusedElement(QString element) {
    if (element == QLatin1String("email"))
        ui->emailEdit->setFocus();
}

QnUserSettingsDialog::ElementFlags QnUserSettingsDialog::elementFlags(Element element) const {
    if(element < 0 || element >= ElementCount) {
        qnWarning("Invalid element '%1'.", static_cast<int>(element));
        return 0;
    }

    return m_flags[element];
}

QnUserResourcePtr QnUserSettingsDialog::user() const {
    return m_user;
}

void QnUserSettingsDialog::setUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    m_user = user;
    if (accessController()->globalPermissions()) {
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
    if(!m_user)
        return;

    if(m_user->getId().isNull()) {
        ui->loginEdit->clear();
        ui->currentPasswordEdit->clear();
        ui->passwordEdit->clear();
        ui->passwordEdit->setPlaceholderText(QString());
        ui->confirmPasswordEdit->clear();
        ui->confirmPasswordEdit->setPlaceholderText(QString());
        ui->emailEdit->clear();

        loadAccessRightsToUi(Qn::GlobalLiveViewerPermissions);
    } else {
        QString placeholder(6, QLatin1Char('*'));

        ui->loginEdit->setText(m_user->getName());
        ui->currentPasswordEdit->clear();
        ui->passwordEdit->setPlaceholderText(placeholder);
        ui->passwordEdit->clear();
        ui->confirmPasswordEdit->setPlaceholderText(placeholder);
        ui->confirmPasswordEdit->clear();
        ui->emailEdit->setText(m_user->getEmail());

        loadAccessRightsToUi(accessController()->globalPermissions(m_user));
        updatePassword();
    }
    setHasChanges(false);
}

void QnUserSettingsDialog::submitToResource() {
    if(!m_user)
        return;

    Qn::Permissions permissions = accessController()->permissions(m_user);

    if (m_userNameModified && (permissions & Qn::WriteNamePermission))
        m_user->setName(ui->loginEdit->text().trimmed());

    if (m_passwordModified && (permissions & Qn::WritePasswordPermission)) {
        m_user->setPassword(ui->passwordEdit->text()); //empty text means 'no change'
        m_user->generateHash();
    }

    /* User cannot change it's own rights */
    if (m_accessRightsModified && (permissions & Qn::WriteAccessRightsPermission)) {
        quint64 rights = ui->accessRightsComboBox->itemData(ui->accessRightsComboBox->currentIndex()).toULongLong();
        if (rights == CUSTOM_RIGHTS)
            rights = readAccessRightsAdvanced();
        m_user->setPermissions(rights);
    }
    if( m_emailModified )
        m_user->setEmail(ui->emailEdit->text());

    setHasChanges(false);
}

void QnUserSettingsDialog::setHint(Element element, const QString &hint) {
    if(m_hints[element] == hint)
        return;

    m_hints[element] = hint;

    auto iter = std::find_if(m_hints.cbegin(), m_hints.cend(), [](const QString &value){return !value.isEmpty();});

    QString actualHint = iter == m_hints.cend() ? QString() : *iter;
    ui->hintLabel->setText(actualHint);
}

void QnUserSettingsDialog::setValid(Element element, bool valid) {
    if(m_valid[element] == valid)
        return;

    m_valid[element] = valid;

    QPalette palette = this->palette();
    if(!valid)
        setWarningStyle(&palette);

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
    case Email:
        ui->emailLabel->setPalette(palette);
        break;
    default:
        break;
    }

    bool allValid = std::all_of(m_valid.cbegin(), m_valid.cend(), [](bool value){return value;});
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(allValid);
}

void QnUserSettingsDialog::updateElement(Element element) {
    bool valid = true;
    QString hint;

    switch(element) {
    case Login:
        if(ui->loginEdit->text().trimmed().isEmpty()) {
            hint = tr("Login cannot be empty.");
            valid = false;
        }
        if(m_userByLogin.contains(ui->loginEdit->text().trimmed().toLower()) && m_userByLogin.value(ui->loginEdit->text().trimmed().toLower()) != m_user) {
            hint = tr("User with specified login already exists.");
            valid = false;
        }
        break;
    case CurrentPassword:
        if((!ui->passwordEdit->text().isEmpty() ||
            !ui->confirmPasswordEdit->text().isEmpty())
                && !m_currentPassword.isEmpty() && ui->currentPasswordEdit->text() != m_currentPassword) {
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
        if (ui->passwordEdit->text() != ui->confirmPasswordEdit->text()) {
            hint = tr("Passwords do not match.");
            valid = false;
        } else if ( (ui->passwordEdit->text().isEmpty() || ui->confirmPasswordEdit->text().isEmpty())
                    && ui->passwordEdit->placeholderText().isEmpty() // creating new user
                  )
        {
            hint = tr("Password cannot be empty.");
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
    case Email:
        if(!ui->emailEdit->text().trimmed().isEmpty() && !QnEmail::isValid(ui->emailEdit->text())) {
            hint = tr("Invalid email address.");
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

void QnUserSettingsDialog::loadAccessRightsToUi(quint64 rights) {
    selectAccessRightsPreset(rights);
    fillAccessRightsAdvanced(rights);
}

void QnUserSettingsDialog::updateAll() {
    updateElement(Email);
    updateElement(AccessRights);
    updateElement(Password);
    updateElement(CurrentPassword);
    updateElement(Login);
}

void QnUserSettingsDialog::updateSizeLimits() {
    QSize hint = sizeHint();
    setMinimumSize(hint);
    setMaximumSize(600, hint.height());
}

void QnUserSettingsDialog::updateDependantPermissions() {
    m_inUpdateDependensies = true;

    bool isOwner = isCheckboxChecked(Qn::ExcludingOwnerPermission);
    if (isOwner){
        setCheckboxEnabled(Qn::ExcludingAdminPermission, false);
        setCheckboxChecked(Qn::ExcludingAdminPermission);
    }

    bool isAdmin = isCheckboxChecked(Qn::ExcludingAdminPermission);
    if (isAdmin){
        setCheckboxEnabled(Qn::GlobalEditCamerasPermission, false);
        setCheckboxChecked(Qn::GlobalEditCamerasPermission);

        setCheckboxEnabled(Qn::GlobalPtzControlPermission, false);
        setCheckboxChecked(Qn::GlobalPtzControlPermission);

        setCheckboxEnabled(Qn::GlobalViewArchivePermission, false);
        setCheckboxChecked(Qn::GlobalViewArchivePermission);

        setCheckboxEnabled(Qn::GlobalExportPermission, false);
        setCheckboxChecked(Qn::GlobalExportPermission);

        setCheckboxEnabled(Qn::GlobalEditVideoWallPermission, false);
        setCheckboxChecked(Qn::GlobalEditVideoWallPermission);
    } else {
        setCheckboxEnabled(Qn::GlobalEditCamerasPermission);
        setCheckboxEnabled(Qn::GlobalPtzControlPermission);
        setCheckboxEnabled(Qn::GlobalViewArchivePermission);
        setCheckboxEnabled(Qn::GlobalExportPermission);
        setCheckboxEnabled(Qn::GlobalEditVideoWallPermission);

        bool canViewArchive = isCheckboxChecked(Qn::GlobalViewArchivePermission);
        setCheckboxEnabled(Qn::GlobalExportPermission, canViewArchive);
        if(!canViewArchive)
            setCheckboxChecked(Qn::GlobalExportPermission, false);
    }

    m_inUpdateDependensies = false;
}

void QnUserSettingsDialog::at_accessRights_changed() {
    if (m_inUpdateDependensies)
        return;

    setHasChanges(true);
    selectAccessRightsPreset(readAccessRightsAdvanced());
    updateDependantPermissions();
}

void QnUserSettingsDialog::createAccessRightsPresets() {
    if (!m_user)
        return;

    Qn::Permissions permissions = accessController()->globalPermissions(m_user);

    // show only for view of owner
    if (permissions & Qn::GlobalEditProtectedUserPermission)
        ui->accessRightsComboBox->addItem(tr("Owner"), Qn::GlobalOwnerPermissions);

    // show for an admin or for anyone opened by owner
    if ((permissions & Qn::GlobalProtectedPermission) || (accessController()->globalPermissions() & Qn::GlobalEditProtectedUserPermission))
        ui->accessRightsComboBox->addItem(tr("Administrator"), Qn::GlobalAdminPermissions);

    ui->accessRightsComboBox->addItem(tr("Advanced Viewer"), Qn::GlobalAdvancedViewerPermissions);
    ui->accessRightsComboBox->addItem(tr("Viewer"), Qn::GlobalViewerPermissions);
    ui->accessRightsComboBox->addItem(tr("Live Viewer"), Qn::GlobalLiveViewerPermissions);

    ui->accessRightsComboBox->addItem(tr("Custom..."), CUSTOM_RIGHTS); // should be the last
}

void QnUserSettingsDialog::createAccessRightsAdvanced() {
    if (!m_user)
        return;

    Qn::Permissions permissions = accessController()->globalPermissions(m_user);
    QWidget* previous = ui->advancedButton;

    if (permissions & Qn::GlobalEditProtectedUserPermission)
        previous = createAccessRightCheckBox(tr("Owner"), Qn::ExcludingOwnerPermission, previous);
    if ((permissions & Qn::GlobalProtectedPermission) || (accessController()->globalPermissions() & Qn::GlobalEditProtectedUserPermission))
        previous = createAccessRightCheckBox(tr("Administrator"),
                     Qn::ExcludingAdminPermission,
                     previous);
    previous = createAccessRightCheckBox(tr("Can adjust camera settings"), Qn::GlobalEditCamerasPermission, previous);
    previous = createAccessRightCheckBox(tr("Can use PTZ controls"), Qn::GlobalPtzControlPermission, previous);
    previous = createAccessRightCheckBox(tr("Can view video archives"), Qn::GlobalViewArchivePermission, previous);
    previous = createAccessRightCheckBox(tr("Can export video"), Qn::GlobalExportPermission, previous);
    previous = createAccessRightCheckBox(tr("Can edit Video Walls"), Qn::GlobalEditVideoWallPermission, previous);  //TODO: #VW #TR

    updateDependantPermissions();
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
    for (int i = 0; i < ui->accessRightsComboBox->count(); i++) {
        if (ui->accessRightsComboBox->itemData(i).toULongLong() == rights) {
            ui->accessRightsComboBox->setCurrentIndex(i);
            custom = false;
            break;
        }
    }

    if (custom) {
        ui->advancedButton->setChecked(true);
        ui->accessRightsComboBox->setCurrentIndex(ui->accessRightsComboBox->count() - 1);
    }
}

void QnUserSettingsDialog::fillAccessRightsAdvanced(quint64 rights) {
    if (m_inUpdateDependensies)
        return; //just in case

    m_inUpdateDependensies = true;

    for(QHash<quint64, QCheckBox *>::const_iterator pos = m_advancedRights.begin(); pos != m_advancedRights.end(); pos++)
        if(pos.value())
            pos.value()->setChecked((pos.key() & rights) == pos.key());
    m_inUpdateDependensies = false;

    updateDependantPermissions(); // TODO: #GDM #Common rename to something more sane, connect properly
}

quint64 QnUserSettingsDialog::readAccessRightsAdvanced() {
    quint64 result = Qn::GlobalViewLivePermission;
    for(QHash<quint64, QCheckBox *>::const_iterator pos = m_advancedRights.begin(); pos != m_advancedRights.end(); pos++)
        if(pos.value() && pos.value()->isChecked())
            result |= pos.key();
    return result;
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
    updateSizeLimits();
}

// Utility functions

bool QnUserSettingsDialog::isCheckboxChecked(quint64 right){
    return m_advancedRights[right] ? m_advancedRights[right]->isChecked() : false;
}

void QnUserSettingsDialog::setCheckboxChecked(quint64 right, bool checked){
    if(QCheckBox *targetCheckBox = m_advancedRights[right]) {
        targetCheckBox->setChecked(checked);
    }
}

void QnUserSettingsDialog::setCheckboxEnabled(quint64 right, bool enabled){
    if(QCheckBox *targetCheckBox = m_advancedRights[right]) {
        targetCheckBox->setEnabled(enabled);
    }
}

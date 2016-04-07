#include "user_settings_dialog.h"
#include "ui_user_settings_dialog.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtGui/QKeyEvent>

#include <common/common_module.h>

#include <core/resource/resource_name.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <utils/common/event_processors.h>
#include <utils/common/warnings.h>
#include <utils/email/email.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <client/client_globals.h>

#include <ui/workbench/watchers/workbench_safemode_watcher.h>

namespace
{
    const int kCustomRights = 0x00FFFFFF;
}

QnUserSettingsDialog::QnUserSettingsDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::UserSettingsDialog()),
    m_mode(Mode::Invalid),
    m_user(0),
    m_hasChanges(false),
    m_inUpdateDependensies(false),
    m_enabledModified(false),
    m_userNameModified(false),
    m_passwordModified(false),
    m_emailModified(false),
    m_accessRightsModified(false)
{
    ui->setupUi(this);

    for(const QnUserResourcePtr &user: qnResPool->getResources<QnUserResource>())
        m_userByLogin[user->getName().toLower()] = user;

    std::fill(m_valid.begin(), m_valid.end(), true);
    std::fill(m_flags.begin(), m_flags.end(), Editable | Visible);

    setHelpTopic(ui->accessRightsLabel, ui->accessRightsComboBox,   Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->accessRightsGroupbox,                          Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->enabledCheckBox,                               Qn::UserSettings_DisableUser_Help);

    ui->accessRightsGroupbox->hide();

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateLogin()));
    connect(ui->currentPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->confirmPasswordEdit,    SIGNAL(textChanged(const QString &)),   this,   SLOT(updatePassword()));
    connect(ui->emailEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(updateEmail()));
    connect(ui->enabledCheckBox,        SIGNAL(clicked(bool)),                  this,   SLOT(updateEnabled()));
    connect(ui->accessRightsComboBox,   SIGNAL(currentIndexChanged(int)),       this,   SLOT(updateAccessRights()));

    connect(ui->loginEdit,              &QLineEdit::textChanged, this,   [this](){ setHasChanges(); m_userNameModified = true; } );
    connect(ui->passwordEdit,           &QLineEdit::textChanged, this,   [this](){ setHasChanges(); m_passwordModified = true; } );
    connect(ui->confirmPasswordEdit,    &QLineEdit::textChanged, this,   [this](){ setHasChanges(); } );
    connect(ui->emailEdit,              &QLineEdit::textChanged, this,   [this](){ setHasChanges(); m_emailModified = true; } );
    connect(ui->enabledCheckBox,        &QCheckBox::clicked,     this,   [this](){ setHasChanges(); m_enabledModified = true; } );
    connect(ui->accessRightsComboBox,   static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this,   [this](){ setHasChanges(); m_accessRightsModified = true; } );

    //connect(ui->advancedButton,         SIGNAL(toggled(bool)),                  ui->accessRightsGroupbox,   SLOT(setVisible(bool)));
    connect(ui->advancedButton,         SIGNAL(toggled(bool)),                  this,   SLOT(at_advancedButton_toggled()));

    connect(ui->accessRightsWidget, &QnAbstractPreferencesWidget::hasChangesChanged, this, [this]() { setHasChanges(); });

    setWarningStyle(ui->hintLabel);

    updateAll();
    updateSizeLimits();

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);
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
        m_enabledModified = false;
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
    case Enabled:
        ui->enabledCheckBox->setVisible(visible);
        setReadOnly(ui->enabledCheckBox, !editable);
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

    if (!user)
        m_mode = Mode::Invalid;
    else if (user->getId().isNull())
        m_mode = Mode::NewUser;
    else if (user == context()->user())
        m_mode = Mode::OwnUser;
    else
        m_mode = Mode::OtherUser;

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

    if(m_mode == Mode::NewUser) {
        ui->loginEdit->clear();
        ui->emailEdit->clear();
        ui->enabledCheckBox->setChecked(true);

        loadAccessRightsToUi(Qn::GlobalLiveViewerPermissions);
    } else {
        ui->loginEdit->setText(m_user->getName());
        ui->emailEdit->setText(m_user->getEmail());
        ui->enabledCheckBox->setChecked(m_user->isEnabled());

        loadAccessRightsToUi(accessController()->globalPermissions(m_user));
    }
    updateLogin();
    updatePassword();

    ec2::ApiAccessRightsData accessRights = qnResourceAccessManager->accessRights(m_user->getId());
    ui->accessRightsWidget->setAccessRights(accessRights);
    ui->accessRightsWidget->loadDataToUi();

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
        int rights = ui->accessRightsComboBox->itemData(ui->accessRightsComboBox->currentIndex()).toInt();
        if (rights == kCustomRights)
            rights = readAccessRightsAdvanced();
        m_user->setPermissions(static_cast<Qn::GlobalPermissions>(rights));
    }
    if( m_emailModified )
        m_user->setEmail(ui->emailEdit->text());

    if (m_enabledModified)
        m_user->setEnabled(ui->enabledCheckBox->isChecked());

    ui->accessRightsWidget->applyChanges();
    ec2::ApiAccessRightsData userAccessRights = ui->accessRightsWidget->accessRights();
    userAccessRights.userId = m_user->getId();
    qnResourceAccessManager->setAccessRights(userAccessRights);

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
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(allValid && !qnCommon->isReadOnly());
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

        /* Show warning message if we have renamed an existing user */
        if (m_mode == Mode::OtherUser)
            updateElement(Password);
        break;
    case CurrentPassword:
        /* Current password should be checked only if the user editing himself. */
        if (m_mode == Mode::OwnUser) {
            if((!ui->passwordEdit->text().isEmpty() ||
                !ui->confirmPasswordEdit->text().isEmpty())
                && !m_currentPassword.isEmpty() && ui->currentPasswordEdit->text() != m_currentPassword) {
                    if(ui->currentPasswordEdit->text().isEmpty()) {
                        hint = tr("To modify your password, please enter existing one.");
                        valid = false;
                    } else {
                        hint = tr("Invalid current password.");
                        valid = false;
                    }
            }
        }
        break;
    case Password:
        /* Show warning message if we have renamed an existing user.. */
        if (m_mode == Mode::OtherUser && ui->loginEdit->text().trimmed() != m_user->getName()) {
            /* ..and have not entered new password. */
            if (ui->passwordEdit->text().isEmpty()) {
                hint = tr("User has been renamed. Password must be updated.");
                valid = false;
            }
        }

        /* Show warning if password and confirmation do not match. */
        if (ui->passwordEdit->text() != ui->confirmPasswordEdit->text()) {
            hint = tr("Passwords do not match.");
            valid = false;
        }

        /* Show warning if have not entered password for the new user. */
        if (m_mode == Mode::NewUser && ui->passwordEdit->text().isEmpty()) {
            hint = tr("Password cannot be empty.");
            valid = false;
        }
        break;
    case AccessRights:
        if(ui->accessRightsComboBox->currentIndex() == -1) {
            hint = tr("Choose access rights.");
            valid = false;
        } else {
            int rights = ui->accessRightsComboBox->itemData(ui->accessRightsComboBox->currentIndex()).toInt();
            if (rights == kCustomRights)
                ui->advancedButton->setChecked(true);
            else
                fillAccessRightsAdvanced(rights);
        }
        break;
    case Email:
        if(!ui->emailEdit->text().trimmed().isEmpty() && !QnEmailAddress::isValid(ui->emailEdit->text())) {
            hint = tr("Invalid email address.");
            valid = false;
        }
        break;
    case Enabled:
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

void QnUserSettingsDialog::loadAccessRightsToUi(int rights) {
    selectAccessRightsPreset(rights);
    fillAccessRightsAdvanced(rights);
}

void QnUserSettingsDialog::updateAll() {
    updateElement(Enabled);
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

    bool isOwner = isCheckboxChecked(Qn::GlobalOwnerPermission);
    if (isOwner)
    {
        setCheckboxEnabled(Qn::GlobalAdminPermission, false);
        setCheckboxChecked(Qn::GlobalAdminPermission);
    }

    bool isAdmin = isCheckboxChecked(Qn::GlobalAdminPermission);
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

void QnUserSettingsDialog::at_customCheckBox_clicked() {
    if (m_inUpdateDependensies)
        return;

    setHasChanges(true);
    m_accessRightsModified = true;

    updateDependantPermissions();
    selectAccessRightsPreset(readAccessRightsAdvanced());
}

void QnUserSettingsDialog::createAccessRightsPresets() {
    if (!m_user)
        return;

    Qn::GlobalPermissions permissions = accessController()->globalPermissions(m_user);

    // show only for view of owner
    if (permissions.testFlag(Qn::GlobalOwnerPermission))
        ui->accessRightsComboBox->addItem(tr("Owner"), Qn::GlobalOwnerPermission);

    // show for an admin or for anyone opened by owner
    if (permissions.testFlag(Qn::GlobalAdminPermission) || accessController()->globalPermissions().testFlag(Qn::GlobalOwnerPermission))
        ui->accessRightsComboBox->addItem(tr("Administrator"), Qn::GlobalAdminPermission);

    ui->accessRightsComboBox->addItem(tr("Advanced Viewer"), Qn::GlobalAdvancedViewerPermissions);
    ui->accessRightsComboBox->addItem(tr("Viewer"), Qn::GlobalViewerPermissions);
    ui->accessRightsComboBox->addItem(tr("Live Viewer"), Qn::GlobalLiveViewerPermissions);

    ui->accessRightsComboBox->addItem(tr("Custom..."), kCustomRights); // should be the last
}

void QnUserSettingsDialog::createAccessRightsAdvanced()
{
    if (!m_user)
        return;

    Qn::GlobalPermissions permissions = accessController()->globalPermissions(m_user);
    QWidget* previous = ui->advancedButton;

    if (permissions.testFlag(Qn::GlobalOwnerPermission))
        previous = createAccessRightCheckBox(tr("Owner"), Qn::GlobalOwnerPermission, previous);

    if (permissions.testFlag(Qn::GlobalAdminPermission) || accessController()->globalPermissions().testFlag(Qn::GlobalOwnerPermission))
        previous = createAccessRightCheckBox(tr("Administrator"),
                     Qn::GlobalAdminPermission,
                     previous);

    previous = createAccessRightCheckBox(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Can adjust devices settings"),
        tr("Can adjust cameras settings")
        ), Qn::GlobalEditCamerasPermission, previous);
    previous = createAccessRightCheckBox(tr("Can use PTZ controls"), Qn::GlobalPtzControlPermission, previous);
    previous = createAccessRightCheckBox(tr("Can view video archives"), Qn::GlobalViewArchivePermission, previous);
    previous = createAccessRightCheckBox(tr("Can export video"), Qn::GlobalExportPermission, previous);
               createAccessRightCheckBox(tr("Can edit Video Walls"), Qn::GlobalEditVideoWallPermission, previous);

    updateDependantPermissions();
}

QCheckBox *QnUserSettingsDialog::createAccessRightCheckBox(QString text, int right, QWidget *previous)
{
    QCheckBox *checkBox = new QCheckBox(text, this);
    ui->accessRightsGroupbox->layout()->addWidget(checkBox);
    m_advancedRights.insert(right, checkBox);
    setTabOrder(previous, checkBox);

    if (isReadOnly(ui->accessRightsGroupbox))
        setReadOnly(checkBox, true);
    else
        connect(checkBox, &QCheckBox::clicked, this, &QnUserSettingsDialog::at_customCheckBox_clicked);
    return checkBox;
}

void QnUserSettingsDialog::selectAccessRightsPreset(int rights)
{
    bool custom = true;
    for (int i = 0; i < ui->accessRightsComboBox->count(); i++)
    {
        if (ui->accessRightsComboBox->itemData(i).toULongLong() == rights)
        {
            ui->accessRightsComboBox->setCurrentIndex(i);
            custom = false;
            break;
        }
    }

    if (custom)
    {
        ui->advancedButton->setChecked(true);
        ui->accessRightsComboBox->setCurrentIndex(ui->accessRightsComboBox->count() - 1);
    }
}

void QnUserSettingsDialog::fillAccessRightsAdvanced(int rights)
{
    if (m_inUpdateDependensies)
        return; //just in case

    m_inUpdateDependensies = true;

    for(auto pos = m_advancedRights.begin(); pos != m_advancedRights.end(); pos++)
        if(pos.value())
            pos.value()->setChecked((pos.key() & rights) == pos.key());
    m_inUpdateDependensies = false;

    updateDependantPermissions(); // TODO: #GDM #Common rename to something more sane, connect properly
}

int QnUserSettingsDialog::readAccessRightsAdvanced()
{
    int result = Qn::GlobalViewLivePermission;
    for(auto pos = m_advancedRights.begin(); pos != m_advancedRights.end(); pos++)
        if(pos.value() && pos.value()->isChecked())
            result |= pos.key();
    return result;
}

void QnUserSettingsDialog::at_advancedButton_toggled()
{
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

bool QnUserSettingsDialog::isCheckboxChecked(int right){
    return m_advancedRights[right] ? m_advancedRights[right]->isChecked() : false;
}

void QnUserSettingsDialog::setCheckboxChecked(int right, bool checked){
    if(QCheckBox *targetCheckBox = m_advancedRights[right]) {
        targetCheckBox->setChecked(checked);
    }
}

void QnUserSettingsDialog::setCheckboxEnabled(int right, bool enabled){
    if(QCheckBox *targetCheckBox = m_advancedRights[right]) {
        targetCheckBox->setEnabled(enabled);
    }
}


#include "user_settings_widget.h"
#include "ui_user_settings_widget.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/common/aligner.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/models/resource_properties/user_settings_model.h>
#include <ui/models/user_roles_model.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/utils/string.h>
#include <utils/email/email.h>


QnUserSettingsWidget::QnUserSettingsWidget(QnUserSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserSettingsWidget()),
    m_model(model),
    m_rolesModel(new QnUserRolesModel(this))
{
    ui->setupUi(this);

    setHelpTopic(ui->roleLabel, ui->roleComboBox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->roleComboBox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->enabledButton, Qn::UserSettings_DisableUser_Help);

    ui->roleComboBox->setModel(m_rolesModel);

    connect(ui->enabledButton,          &QPushButton::clicked,          this,   &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->roleComboBox,           QnComboboxCurrentIndexChanged,  this,   &QnUserSettingsWidget::hasChangesChanged);

    connect(m_rolesModel,               &QnUserRolesModel::modelReset,  this,   &QnUserSettingsWidget::updateRoleComboBox);
    connect(m_rolesModel,               &QnUserRolesModel::rowsRemoved, this,   &QnUserSettingsWidget::updateRoleComboBox);

    setupInputFields();

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());

    for (auto field: inputFields())
        aligner->addWidget(field);

    aligner->addWidget(ui->roleLabel);
    layout()->activate();
}

void QnUserSettingsWidget::updatePermissionsLabel(const QString& text)
{
    ui->permissionsLabel->setText(text);
}

QnUserSettingsWidget::~QnUserSettingsWidget()
{
}

bool QnUserSettingsWidget::hasChanges() const
{
    if (m_model->mode() == QnUserSettingsModel::NewUser)
        return true;

    if (!validMode())
        return false;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (permissions.testFlag(Qn::WriteNamePermission))
        if (m_model->user()->getName() != ui->loginInputField->text().trimmed())
            return true;

    if (permissions.testFlag(Qn::WritePasswordPermission)
        && !ui->passwordInputField->text().isEmpty()
        && !m_model->user()->checkPassword(ui->passwordInputField->text())
        )
        return true;

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        if (m_model->user()->isEnabled() != ui->enabledButton->isChecked())
            return true;

        QnUuid groupId = selectedUserGroup();
        if (m_model->user()->userGroup() != groupId)
            return true;

        if (groupId.isNull())
        {
            /* Check if we have selected a predefined internal group. */
            Qn::GlobalPermissions permissions = selectedPermissions();
            if (permissions != Qn::NoGlobalPermissions
                && permissions != qnResourceAccessManager->globalPermissions(m_model->user()))
            {
                return true;
            }
        }
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        if (m_model->user()->getEmail() != ui->emailInputField->text())
            return true;

    if (permissions.testFlag(Qn::WriteFullNamePermission))
        if (m_model->user()->fullName() != ui->nameInputField->text().trimmed())
            return true;

    return false;
}

void QnUserSettingsWidget::loadDataToUi()
{
    if (!validMode())
        return;

    updateRoleComboBox();
    updateControlsAccess();

    ui->passwordInputField->setValidator(Qn::defaultPasswordValidator(
        m_model->mode() != QnUserSettingsModel::NewUser));

    ui->loginInputField->setText(m_model->user()->getName());
    ui->emailInputField->setText(m_model->user()->getEmail());
    ui->nameInputField->setText(m_model->user()->fullName());
    ui->passwordInputField->clear();
    ui->confirmPasswordInputField->clear();
    ui->enabledButton->setChecked(m_model->user()->isEnabled());

    for (auto field : inputFields())
        field->reset();

    ui->loginInputField->setFocus();
}

void QnUserSettingsWidget::applyChanges()
{
    if (!validMode())
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (permissions.testFlag(Qn::WriteNamePermission))
        m_model->user()->setName(ui->loginInputField->text().trimmed());

    //empty text means 'no change'
    const QString newPassword = ui->passwordInputField->text().trimmed();
    if (permissions.testFlag(Qn::WritePasswordPermission) && !newPassword.isEmpty()) //TODO: #GDM #access implement correct check
    {
        m_model->user()->setPassword(newPassword);
        m_model->user()->generateHash();
        m_model->user()->setPassword(QString());
    }

    /* Here we must be sure settings widget goes before the permissions one. */
    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        m_model->user()->setUserGroup(selectedUserGroup());
        m_model->user()->setRawPermissions(selectedPermissions());
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        m_model->user()->setEmail(ui->emailInputField->text().trimmed());

    if (permissions.testFlag(Qn::WriteFullNamePermission))
        m_model->user()->setFullName(ui->nameInputField->text().trimmed());

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
        m_model->user()->setEnabled(ui->enabledButton->isChecked());
}

bool QnUserSettingsWidget::canApplyChanges() const
{
    if (!validMode())
        return true;

    for (auto field : inputFields())
        if (!field->isValid())
            return false;

    return true;
}

void QnUserSettingsWidget::setupInputFields()
{
    ui->loginInputField->setTitle(tr("Login"));
    ui->loginInputField->setValidator([this](const QString& text)
    {
        if (text.trimmed().isEmpty())
            return Qn::ValidationResult(tr("Login cannot be empty."));

        for (const QnUserResourcePtr& user : qnResPool->getResources<QnUserResource>())
        {
            if (user == m_model->user())
                continue;

            if (user->getName().toLower() != text.toLower())
                continue;

            return Qn::ValidationResult(tr("User with specified login already exists."));
        }

        return Qn::kValidResult;
    });

    auto updatePasswordValidator = [this]
        {
            /* Check if we must update password for the other user. */
            if (m_model->mode() == QnUserSettingsModel::OtherSettings && m_model->user())
            {
                bool mustUpdatePassword = ui->loginInputField->text() != m_model->user()->getName();

                ui->passwordInputField->setValidator(
                    Qn::defaultPasswordValidator(
                        !mustUpdatePassword,
                        tr("User has been renamed. Password must be updated.")
                    ),
                    false);
            }
        };

    connect(ui->loginInputField, &QnInputField::textChanged,     this, updatePasswordValidator);
    connect(ui->loginInputField, &QnInputField::editingFinished, this, updatePasswordValidator);
    connect(ui->loginInputField, &QnInputField::editingFinished, ui->passwordInputField, &QnInputField::validate);

    ui->nameInputField->setTitle(tr("Name"));

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator(Qn::defaultEmailValidator());

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setEchoMode(QLineEdit::Password);
    ui->passwordInputField->setPasswordIndicatorEnabled(true);

    connect(ui->passwordInputField, &QnInputField::textChanged, this, [this]()
    {
        if (!ui->confirmPasswordInputField->text().isEmpty())
        ui->confirmPasswordInputField->validate();
    });

    connect(ui->passwordInputField, &QnInputField::editingFinished,
        ui->confirmPasswordInputField, &QnInputField::validate);

    ui->confirmPasswordInputField->setTitle(tr("Confirm Password"));
    ui->confirmPasswordInputField->setEchoMode(QLineEdit::Password);
    ui->confirmPasswordInputField->setValidator(Qn::defaultConfirmationValidator(
        [this]() { return ui->passwordInputField->text(); },
        tr("Passwords do not match.")));

    for (auto field : inputFields())
        connect(field, &QnInputField::textChanged, this, &QnUserSettingsWidget::hasChangesChanged);
}

QList<QnInputField*> QnUserSettingsWidget::inputFields() const
{
    return {
        ui->loginInputField,
        ui->nameInputField,
        ui->passwordInputField,
        ui->confirmPasswordInputField,
        ui->emailInputField
    };
}

void QnUserSettingsWidget::updateRoleComboBox()
{
    if (!m_model->user())
        return;

    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_model->user());

    /* If there is only one entry in permissions combobox, this check doesn't matter. */
    int customPermissionsIndex = ui->roleComboBox->count() - 1;
    NX_ASSERT(customPermissionsIndex == 0 ||
        ui->roleComboBox->itemData(customPermissionsIndex, Qn::GlobalPermissionsRole).value<Qn::GlobalPermissions>() == Qn::NoGlobalPermissions);
    NX_ASSERT(customPermissionsIndex == 0 ||
        ui->roleComboBox->itemData(customPermissionsIndex, Qn::UuidRole).value<QnUuid>().isNull());

    int permissionsIndex = customPermissionsIndex;
    if (!m_model->user()->userGroup().isNull())
    {
        permissionsIndex = ui->roleComboBox->findData(qVariantFromValue(m_model->user()->userGroup()), Qn::UuidRole, Qt::MatchExactly);
    }
    else if (permissions != Qn::NoGlobalPermissions)
    {
        permissionsIndex = ui->roleComboBox->findData(qVariantFromValue(permissions), Qn::GlobalPermissionsRole, Qt::MatchExactly);
    }

    if (permissionsIndex < 0)
        permissionsIndex = customPermissionsIndex;

    ui->roleComboBox->setCurrentIndex(permissionsIndex);
}

void QnUserSettingsWidget::updateControlsAccess()
{
    Qn::Permissions permissions = m_model->user()
        ? accessController()->permissions(m_model->user())
        : Qn::NoPermissions;

    ui->loginInputField->setReadOnly(!permissions.testFlag(Qn::WriteNamePermission));
    ui->passwordInputField->setVisible(permissions.testFlag(Qn::WritePasswordPermission));
    ui->confirmPasswordInputField->setVisible(permissions.testFlag(Qn::WritePasswordPermission));
    ui->emailInputField->setReadOnly(!permissions.testFlag(Qn::WriteEmailPermission));
    ui->nameInputField->setReadOnly(!permissions.testFlag(Qn::WriteFullNamePermission));
    ui->enabledButton->setVisible(permissions.testFlag(Qn::WriteAccessRightsPermission));
}

Qn::GlobalPermissions QnUserSettingsWidget::selectedPermissions() const
{
    return ui->roleComboBox->itemData(ui->roleComboBox->currentIndex(), Qn::GlobalPermissionsRole).value<Qn::GlobalPermissions>();
}

QnUuid QnUserSettingsWidget::selectedUserGroup() const
{
    return ui->roleComboBox->itemData(ui->roleComboBox->currentIndex(), Qn::UuidRole).value<QnUuid>();
}

bool QnUserSettingsWidget::validMode() const
{
    return m_model->mode() == QnUserSettingsModel::OtherSettings
        || m_model->mode() == QnUserSettingsModel::NewUser;
}

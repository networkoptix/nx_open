#include "user_settings_widget.h"
#include "ui_user_settings_widget.h"

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/resource_access_manager.h>

#include <core/resource/user_resource.h>

#include <ui/common/aligner.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/models/resource_properties/user_settings_model.h>
#include <ui/models/user_roles_model.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/utils/string.h>

#include <utils/common/app_info.h>
#include <utils/email/email.h>

namespace {

const int kCloudUserFontSizePixels = 18;
const int kCloudUserFontWeight = QFont::Light;

enum
{
    kLocalIndex,
    kCloudIndex
};

QString processedEmail(const QString& source)
{
    QnEmailAddress addr(source);
    if (!addr.isValid())
        return source;

    return addr.value();
}

} // unnamed namespace


QnUserSettingsWidget::QnUserSettingsWidget(QnUserSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserSettingsWidget()),
    m_model(model),
    m_rolesModel(new QnUserRolesModel(this, QnUserRolesModel::AllRoleFlags)),
    m_aligner(new QnAligner(this))
{
    ui->setupUi(this);

    ui->userTypeComboBox->insertItem(kLocalIndex, tr("Local"));
    ui->userTypeComboBox->insertItem(kCloudIndex, tr("Cloud"));
    ui->cloudPanelWidget->setOptions(QnCloudUserPanelWidget::ShowEnableButtonOption);

    setHelpTopic(ui->roleLabel, ui->roleComboBox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->roleComboBox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->enabledButton, Qn::UserSettings_DisableUser_Help);

    ui->roleComboBox->setModel(m_rolesModel);

    connect(ui->enabledButton,          &QPushButton::toggled,          this,   &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->roleComboBox,           QnComboboxCurrentIndexChanged,  this,   &QnUserSettingsWidget::hasChangesChanged);

    connect(m_rolesModel,               &QnUserRolesModel::modelReset,  this,   &QnUserSettingsWidget::updateRoleComboBox);
    connect(m_rolesModel,               &QnUserRolesModel::rowsRemoved, this,   &QnUserSettingsWidget::updateRoleComboBox);

    /* Synchronize controls for local and cloud users for convenience (local controls are master controls): */
    connect(ui->enabledButton,          &QPushButton::toggled,          this,
        [this](bool on) { ui->cloudPanelWidget->setEnabled(on); });

    connect(ui->cloudPanelWidget, &QnCloudUserPanelWidget::enabledChanged, this,
        [this](bool value) { ui->enabledButton->setChecked(value); });

    connect(ui->loginInputField,        &QnInputField::textChanged,     this,
        [this](const QString& text)
        {
            ui->cloudPanelWidget->setEmail(text);
            ui->cloudEmailInputField->setText(processedEmail(text));
        });

    connect(ui->nameInputField,         &QnInputField::textChanged,     this,
        [this](const QString& text)
        {
            ui->cloudPanelWidget->setFullName(text);
        });

    emit ui->loginInputField->textChanged(QString());
    emit ui->nameInputField->textChanged(QString());

    connect(ui->userTypeComboBox,       QnComboboxCurrentIndexChanged,  this,
        [this](int index)
        {
            ui->secondaryStackedWidget->setCurrentIndex(index);
            if (m_model->mode() == QnUserSettingsModel::NewUser)
            {
                emit userTypeChanged(index > 0);
                emit hasChangesChanged();
            }
        });

    auto stackedWidgetCurrentChanged = [this](int index)
    {
        auto stackedWidget = static_cast<QStackedWidget*>(sender());
        for (int i = 0; i < stackedWidget->count(); ++i)
        {
            auto policy = i == index ? QSizePolicy::Maximum : QSizePolicy::Ignored;
            stackedWidget->widget(i)->setSizePolicy(QSizePolicy::Expanding, policy);
            stackedWidget->widget(i)->adjustSize();
        }

        stackedWidget->adjustSize();
    };

    connect(ui->mainStackedWidget,      &QStackedWidget::currentChanged, this, stackedWidgetCurrentChanged);
    connect(ui->secondaryStackedWidget, &QStackedWidget::currentChanged, this, stackedWidgetCurrentChanged);

    setupInputFields();

    m_aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    m_aligner->setSkipInvisible(true);

    for (auto field: inputFields())
        m_aligner->addWidget(field);

    m_aligner->addWidget(ui->roleLabel);
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

    if (permissions.testFlag(Qn::WriteNamePermission)
        && m_model->user()->getName() != ui->loginInputField->text().trimmed())
    {
        return true;
    }

    if (permissions.testFlag(Qn::WritePasswordPermission)
        && !ui->passwordInputField->text().isEmpty()
        && !m_model->user()->checkLocalUserPassword(ui->passwordInputField->text()))
    {
        return true;
    }

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
            Qn::UserRole roleType = selectedRole();
            if (roleType != Qn::UserRole::CustomPermissions
                && (QnUserRolesManager::userRolePermissions(roleType)
                    != qnResourceAccessManager->globalPermissions(m_model->user())))
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

    ui->userTypeComboBox->setCurrentIndex(0);

    if (m_model->mode() == QnUserSettingsModel::NewUser)
    {
        bool localSystem = qnGlobalSettings->cloudSystemId().isEmpty();
        ui->userTypeWidget->setHidden(localSystem);
        ui->mainStackedWidget->setCurrentWidget(ui->localUserPage);
        ui->userTypeComboBox->setCurrentIndex(localSystem ? kLocalIndex : kCloudIndex);
    }
    else if (m_model->mode() == QnUserSettingsModel::OtherSettings)
    {
        ui->userTypeWidget->setHidden(true);
        ui->mainStackedWidget->setCurrentWidget(
            m_model->user()->isCloud() ? ui->cloudUserPage : ui->localUserPage);
    }

    updateRoleComboBox();
    updateControlsAccess();

    ui->passwordInputField->setValidator(Qn::defaultPasswordValidator(
        m_model->mode() != QnUserSettingsModel::NewUser));

    ui->loginInputField->setText(m_model->user()->getName());
    ui->emailInputField->setText(m_model->user()->getEmail());
    ui->nameInputField->setText(m_model->user()->fullName());
    ui->cloudEmailInputField->setText(m_model->user()->getEmail());
    ui->cloudPanelWidget->setEmail(m_model->user()->getEmail());
    ui->passwordInputField->clear();
    ui->confirmPasswordInputField->clear();
    ui->enabledButton->setChecked(m_model->user()->isEnabled());

    ui->loginInputField->setFocus();

    for (auto field : inputFields())
        field->reset();
}

void QnUserSettingsWidget::applyChanges()
{
    if (!validMode())
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (m_model->user()->isCloud())
    {
        if (m_model->mode() == QnUserSettingsModel::NewUser)
        {
            NX_ASSERT(permissions.testFlag(Qn::WriteNamePermission));
            NX_ASSERT(permissions.testFlag(Qn::WriteEmailPermission));

            QnEmailAddress addr(ui->cloudEmailInputField->text());
            NX_ASSERT(addr.isValid());
            if (!addr.isValid())
                return;

            const QString cloudLogin = addr.value();
            m_model->user()->setName(cloudLogin);
            m_model->user()->setEmail(cloudLogin);
        }
    }
    else
    {
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

        if (permissions.testFlag(Qn::WriteEmailPermission))
            m_model->user()->setEmail(ui->emailInputField->text().trimmed());

        if (permissions.testFlag(Qn::WriteFullNamePermission))
            m_model->user()->setFullName(ui->nameInputField->text().trimmed());
    }

    /* Here we must be sure settings widget goes before the permissions one. */
    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        m_model->user()->setUserGroup(selectedUserGroup());
        if (selectedRole() != Qn::UserRole::CustomPermissions)
            m_model->user()->setRawPermissions(QnUserRolesManager::userRolePermissions(selectedRole()));
    }

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
        m_model->user()->setEnabled(ui->enabledButton->isChecked());
}

bool QnUserSettingsWidget::canApplyChanges() const
{
    auto checkFields = [](QList<QnInputField*> fields)
        {
            return std::all_of(fields.cbegin(), fields.cend(),
                 [](QnInputField* field)
                 {
                     return field->isValid();
                 });
        };

    const QList<QnInputField*> kLocalUserFields{
        ui->loginInputField,
        ui->nameInputField,
        ui->passwordInputField,
        ui->confirmPasswordInputField,
        ui->emailInputField
    };

    switch (m_model->mode())
    {
        case QnUserSettingsModel::NewUser:
        {
            switch (ui->userTypeComboBox->currentIndex())
            {
                case kCloudIndex:
                    return checkFields({ui->cloudEmailInputField});
                case kLocalIndex:
                    return checkFields(kLocalUserFields);
                default:
                    break;
            }
            break;
        }
        case QnUserSettingsModel::OtherSettings:
        {
            if (m_model->user()->isCloud())
                return true;

            return checkFields(kLocalUserFields);
        }
        default:
            break;
    }

    NX_ASSERT(false);
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
                        tr("User has been renamed. Password must be updated.")),
                    false);
            }
        };

    connect(ui->loginInputField, &QnInputField::textChanged,     this, updatePasswordValidator);
    connect(ui->loginInputField, &QnInputField::editingFinished, this, [this]()
    {
        if (ui->loginInputField->isValid() && m_model->mode() == QnUserSettingsModel::OtherSettings)
        {
            bool passwordWasValid = ui->passwordInputField->lastValidationResult() != QValidator::Invalid;
            if (ui->passwordInputField->isValid() != passwordWasValid)
                ui->passwordInputField->validate();
        }
    });

    ui->nameInputField->setTitle(tr("Name"));

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator(Qn::defaultEmailValidator());

    ui->cloudEmailInputField->setTitle(tr("Email"));
    ui->cloudEmailInputField->setValidator([this](const QString& text)
    {
        if (m_model->mode() != QnUserSettingsModel::NewUser)
            return Qn::kValidResult;

        auto result = Qn::defaultNonEmptyValidator(tr("Email cannot be empty."))(text);
        if (result.state != QValidator::Acceptable)
            return result;

        auto email = text.trimmed().toLower();
        for (const auto& user : qnResPool->getResources<QnUserResource>())
        {
            if (!user->isCloud())
                continue;

            if (user->getEmail().toLower() != email)
                continue;

            return Qn::ValidationResult(tr("Cloud user with specified email already exists."));
        }

        result = Qn::defaultEmailValidator()(text);
        return result;
    });
    connect(ui->cloudEmailInputField, &QnInputField::textChanged, this,
        [this]
        {
            ui->cloudEmailInputField->setText(processedEmail(ui->cloudEmailInputField->text()));
            ui->cloudEmailInputField->validate();
        });

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
        ui->emailInputField,
        ui->cloudEmailInputField
    };
}

void QnUserSettingsWidget::updateRoleComboBox()
{
    if (!m_model->user())
        return;

    ui->roleComboBox->setCurrentIndex(m_rolesModel->rowForUser(m_model->user()));
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

Qn::UserRole QnUserSettingsWidget::selectedRole() const
{
    return ui->roleComboBox->itemData(ui->roleComboBox->currentIndex(), Qn::UserRoleRole).value<Qn::UserRole>();
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

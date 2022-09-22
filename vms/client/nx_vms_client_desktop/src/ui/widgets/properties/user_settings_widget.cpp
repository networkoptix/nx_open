// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_settings_widget.h"
#include "ui_user_settings_widget.h"

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>
#include <nx/branding.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_properties/user_settings_model.h>
#include <ui/models/user_roles_model.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/email/email.h>

using namespace nx::vms::client::desktop;

namespace {

const int kCloudUserFontSizePixels = 18;
const auto kCloudUserFontWeight = QFont::Light;

const int kUserTypeComboMinimumWidth = 120;

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

static const QString kAllowedLoginSymbols = "!#$%&'()*+,-./:;<=>?[]^_`{|}~";

bool isAcceptedLoginCharacter(QChar character)
{
    return character.isLetterOrNumber()
        || character == ' '
        || kAllowedLoginSymbols.contains(character);
}

TextValidateFunction digestPasswordValidator(QnUserSettingsModel* const model)
{
    return
        [model](const QString& text)
        {
            if (text.isEmpty()
                && (model->digestSupport() == QnUserResource::DigestSupport::enable)
                && (model->mode() != QnUserSettingsModel::Mode::NewUser))
            {
                return ValidationResult({}); //< Error, without a message.
            }

            return ValidationResult::kValid;
        };
}

TextValidateFunction digestConfirmationValidator(QnUserSettingsModel* const model)
{
    return
        [model](const QString& text)
        {
            if (text.isEmpty()
                && (model->digestSupport() == QnUserResource::DigestSupport::enable)
                && (model->mode() != QnUserSettingsModel::Mode::NewUser))
            {
                // TODO: Fix text when specification will be updated.
                return ValidationResult(QnUserSettingsWidget::tr("Password reset is required"));
            }

            return ValidationResult::kValid;
        };
}

} // unnamed namespace

QnUserSettingsWidget::QnUserSettingsWidget(QnUserSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserSettingsWidget()),
    m_model(model),
    m_rolesModel(new QnUserRolesModel(this, QnUserRolesModel::DefaultRoleFlags)),
    m_aligner(new Aligner(this)),
    m_lastUserTypeIndex(kCloudIndex) //< actual only for cloud systems (when selector is visible)
{
    using namespace nx::vms::common;

    ui->setupUi(this);

    m_localInputFields = {
        ui->loginInputField,
        ui->nameInputField,
        ui->passwordInputField,
        ui->confirmPasswordInputField,
        ui->emailInputField };

    m_cloudInputFields = {
        ui->cloudEmailInputField };

    ui->userTypeComboBox->insertItem(kLocalIndex, tr("Local"));
    ui->userTypeComboBox->insertItem(kCloudIndex, nx::branding::shortCloudName());
    ui->userTypeComboBox->setMinimumWidth(qMax(kUserTypeComboMinimumWidth,
        ui->userTypeComboBox->minimumSizeHint().width()));

    ui->cloudPanelWidget->setManageLinkShown(false);

    setHelpTopic(ui->roleLabel, ui->roleComboBox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->roleComboBox, Qn::UserSettings_UserRoles_Help);

    const QString localUsers = html::bold(tr("Local users"));
    const QString cloudUsers = html::bold(tr("%1 users", "%1 is the short cloud name (like Cloud)")
        .arg(nx::branding::shortCloudName()));

    ui->userTypeLabel->addHintLine(
        tr("%1 belong to this system only and are fully managed by system administrators.",
            "%1 is local users definition, e.g. 'Local users'")
        .arg(localUsers));
    ui->userTypeLabel->addHintLine(
        tr("%1 can have access to many Systems. Administrators can manage their rights only.",
            "%1 is cloud users definition, e.g. 'Cloud users'")
        .arg(cloudUsers));

    setHelpTopic(ui->userTypeLabel, Qn::NewUser_Help);

    ui->roleComboBox->setModel(m_rolesModel);

    connect(ui->roleComboBox, QnComboboxCurrentIndexChanged,
        this, &QnUserSettingsWidget::hasChangesChanged);

    connect(m_rolesModel, &QnUserRolesModel::modelReset,
        this, &QnUserSettingsWidget::updateRoleComboBox);
    connect(m_rolesModel, &QnUserRolesModel::rowsRemoved,
        this, &QnUserSettingsWidget::updateRoleComboBox);

    connect(ui->loginInputField, &InputField::textChanged, this,
        [this](const QString& text)
        {
            ui->cloudPanelWidget->setEmail(text);
            ui->cloudEmailInputField->setText(processedEmail(text));
        });

    connect(ui->nameInputField, &InputField::textChanged, this,
        [this](const QString& text)
        {
            ui->cloudPanelWidget->setFullName(text);
        });

    emit ui->loginInputField->textChanged(QString());
    emit ui->nameInputField->textChanged(QString());

    connect(ui->userTypeComboBox, QnComboboxCurrentIndexChanged, this,
        [this](int index)
        {
            ui->secondaryStackedWidget->setCurrentIndex(index);
            if (m_model->mode() == QnUserSettingsModel::NewUser)
            {
                emit userTypeChanged(index > 0);
                emit hasChangesChanged();
            }
        });

    connect(ui->editRolesButton, &QPushButton::clicked, this,
        [this]
        {
            menu()->trigger(ui::action::UserRolesAction, ui::action::Parameters()
                .withArgument(Qn::UuidRole, selectedUserRoleId())
                .withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this)));
        });

    connect(model, &QnUserSettingsModel::digestSupportChanged, this,
        [this]
        {
            if (m_model->digestSupport() == QnUserResource::DigestSupport::enable)
            {
                ui->passwordInputField->validate();
                ui->confirmPasswordInputField->validate();
                updatePasswordPlaceholders();
            }
        });

    autoResizePagesToContents(ui->mainStackedWidget,
        { QSizePolicy::Expanding, QSizePolicy::Maximum }, true);
    autoResizePagesToContents(ui->secondaryStackedWidget,
        { QSizePolicy::Expanding, QSizePolicy::Maximum }, true);

    setupInputFields();

    m_aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    m_aligner->registerTypeAccessor<QnCloudUserPanelWidget>(
        QnCloudUserPanelWidget::createIconWidthAccessor());
    m_aligner->setSkipInvisible(false);
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
        && !ui->passwordInputField->text().isEmpty())
    {
        return true;
    }

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        const auto selectedRole = this->selectedRole();
        const bool changed = selectedRole == Qn::UserRole::customUserRole
            ? m_model->user()->firstRoleId() != selectedUserRoleId()
            : m_model->user()->userRole() != selectedRole;
        if (changed)
            return true;
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
    {
        if (m_model->user()->getEmail() != ui->emailInputField->text())
            return true;
    }

    if (permissions.testFlag(Qn::WriteFullNamePermission))
    {
        if (m_model->user()->fullName() != ui->nameInputField->text().trimmed())
            return true;
    }

    return false;
}

void QnUserSettingsWidget::loadDataToUi()
{
    if (!validMode())
        return;

    ui->userTypeComboBox->setCurrentIndex(0);

    if (m_model->mode() == QnUserSettingsModel::NewUser)
    {
        bool localSystem = globalSettings()->cloudSystemId().isEmpty();
        ui->userTypeWidget->setHidden(localSystem);
        ui->mainStackedWidget->setCurrentWidget(ui->localUserPage);
        ui->userTypeComboBox->setCurrentIndex(localSystem ? kLocalIndex : m_lastUserTypeIndex);
    }
    else if (m_model->mode() == QnUserSettingsModel::OtherSettings)
    {
        ui->userTypeWidget->setHidden(true);
        ui->mainStackedWidget->setCurrentWidget(
            m_model->user()->isCloud() ? ui->cloudUserPage : ui->localUserPage);
    }

    updateRoleComboBox();
    updateControlsAccess();

    ui->passwordInputField->setValidator(
        validatorsConcatenator(
        {
            defaultPasswordValidator(m_model->mode() != QnUserSettingsModel::NewUser),
            digestPasswordValidator(m_model)
        }));

    ui->loginInputField->setText(m_model->user()->getName());
    ui->emailInputField->setText(m_model->user()->getEmail());
    ui->nameInputField->setText(m_model->user()->fullName());
    ui->cloudEmailInputField->setText(m_model->user()->getEmail());
    ui->cloudPanelWidget->setEmail(m_model->user()->getEmail());
    ui->passwordInputField->clear();
    ui->confirmPasswordInputField->clear();

    updatePasswordPlaceholders();

    ui->loginInputField->setFocus();

    m_aligner->clear();

    for (auto field: relevantInputFields())
    {
        field->reset();
        m_aligner->addWidget(field);
    }

    m_aligner->addWidget(ui->roleLabel);

    if (m_model->mode() == QnUserSettingsModel::NewUser)
        m_aligner->addWidget(ui->userTypeLabel);
    else if (m_model->user()->isCloud())
        m_aligner->addWidget(ui->cloudPanelWidget);
}

QString QnUserSettingsWidget::passwordPlaceholder() const
{
    if (m_model->mode() != QnUserSettingsModel::OtherSettings)
        return QString();

    static const int kPasswordPlaceholderLength = 10;
    const QChar kPasswordChar = QChar(qApp->style()->styleHint(
        QStyle::SH_LineEdit_PasswordCharacter));

    return QString(kPasswordPlaceholderLength, kPasswordChar);
}

void QnUserSettingsWidget::updatePasswordPlaceholders()
{
    const bool showPlaceholders = ui->passwordInputField->text().isEmpty()
        && ui->confirmPasswordInputField->text().isEmpty()
        && (m_model->digestSupport() == QnUserResource::DigestSupport::keep)
        && !mustUpdatePassword();

    const QString placeholderText = showPlaceholders
            ? passwordPlaceholder()
            : QString();

    ui->passwordInputField->setPlaceholderText(placeholderText);
    ui->confirmPasswordInputField->setPlaceholderText(placeholderText);
}

void QnUserSettingsWidget::applyChanges()
{
    if (!validMode())
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = (m_model->mode() == QnUserSettingsModel::NewUser)
        ? Qn::FullUserPermissions
        : accessController()->permissions(m_model->user());

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
        // TODO: #sivanov Implement correct check.
        if (permissions.testFlag(Qn::WritePasswordPermission) && !newPassword.isEmpty())
        {
            m_model->user()->setPasswordAndGenerateHash(newPassword, m_model->digestSupport());
            m_model->user()->resetPassword();
        }

        if (permissions.testFlag(Qn::WriteEmailPermission))
            m_model->user()->setEmail(ui->emailInputField->text().trimmed());

        if (permissions.testFlag(Qn::WriteFullNamePermission))
            m_model->user()->setFullName(ui->nameInputField->text().trimmed());
    }

    /* Here we must be sure settings widget goes before the permissions one. */
    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        m_model->user()->setSingleUserRole(selectedUserRoleId()); //< TODO: Handle more than 1 role.

        // We must set special 'Custom' flag for the users to avoid collisions with built-in roles.
        m_model->user()->setRawPermissions(selectedRole() == Qn::UserRole::customPermissions
            ? GlobalPermission::customUser
            : QnUserRolesManager::userRolePermissions(selectedRole()));
    }

    if (!ui->userTypeWidget->isHidden())
        m_lastUserTypeIndex = ui->userTypeComboBox->currentIndex();
}

bool QnUserSettingsWidget::canApplyChanges() const
{
    auto checkFields = [](QList<InputField*> fields)
        {
            return std::all_of(fields.cbegin(), fields.cend(),
                 [](InputField* field)
                 {
                     return field->isValid();
                 });
        };

    switch (m_model->mode())
    {
        case QnUserSettingsModel::NewUser:
        {
            switch (ui->userTypeComboBox->currentIndex())
            {
                case kCloudIndex:
                    return checkFields(cloudInputFields());
                case kLocalIndex:
                    return checkFields(localInputFields());
                default:
                    break;
            }
            break;
        }
        case QnUserSettingsModel::OtherSettings:
        {
            if (m_model->user()->isCloud() || m_model->user()->isLdap())
                return true;

            return checkFields(localInputFields());
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
    ui->loginInputField->setValidator(
        [this](const QString& text)
        {
            if (text.trimmed().isEmpty())
                return ValidationResult(tr("Login cannot be empty."));

            if (!std::all_of(text.cbegin(), text.cend(), &isAcceptedLoginCharacter))
            {
                return ValidationResult(
                    tr("Only letters, numbers and symbols %1 are allowed.")
                        .arg(kAllowedLoginSymbols));
            }

            for (const auto& user: resourcePool()->getResources<QnUserResource>())
            {
                if (user == m_model->user())
                    continue;

                if (user->getName().toLower() != text.toLower())
                    continue;

                return ValidationResult(tr("User with specified login already exists."));
            }

            return ValidationResult::kValid;
        });

    auto updatePasswordValidator =
        [this]()
        {
            /* Check if we must update password for the other user. */
            if (m_model->mode() == QnUserSettingsModel::OtherSettings && m_model->user())
            {
                updatePasswordPlaceholders();

                ui->passwordInputField->setValidator(
                    validatorsConcatenator(
                    {
                        defaultPasswordValidator(
                            !mustUpdatePassword(),
                            tr("User has been renamed. Password must be updated.")),
                        digestPasswordValidator(m_model)
                    }),
                    false);
            }
        };

    connect(ui->loginInputField, &InputField::textChanged, this, updatePasswordValidator);
    connect(ui->loginInputField, &InputField::editingFinished, this,
        [this]()
        {
            if (ui->loginInputField->isValid() && m_model->mode() == QnUserSettingsModel::OtherSettings)
            {
                bool passwordWasValid = ui->passwordInputField->lastValidationResult() != QValidator::Invalid;
                if (ui->passwordInputField->isValid() != passwordWasValid)
                    ui->passwordInputField->updateDisplayStateDelayed();
            }
        });

    ui->nameInputField->setTitle(tr("Name"));

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator(defaultEmailValidator());

    ui->cloudEmailInputField->setTitle(tr("Email"));
    ui->cloudEmailInputField->setValidator(
        [this](const QString& text)
        {
            if (m_model->mode() != QnUserSettingsModel::NewUser)
                return ValidationResult::kValid;

            auto result = defaultNonEmptyValidator(tr("Email cannot be empty."))(text);
            if (result.state != QValidator::Acceptable)
                return result;

            auto email = text.trimmed().toLower();
            for (const auto& user : resourcePool()->getResources<QnUserResource>())
            {
                if (!user->isCloud())
                    continue;

                if (user->getEmail().toLower() != email)
                    continue;

                return ValidationResult(
                    tr("%1 user with specified email already exists.",
                        "%1 is the short cloud name (like Cloud)")
                    .arg(nx::branding::shortCloudName()));
            }

            result = defaultEmailValidator()(text);
            return result;
        });

    connect(ui->cloudEmailInputField, &InputField::textChanged, this,
        [this]
        {
            ui->cloudEmailInputField->setText(processedEmail(ui->cloudEmailInputField->text()));
            ui->cloudEmailInputField->validate();
        });

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setPasswordIndicatorEnabled(true);

    connect(ui->passwordInputField, &InputField::textChanged, this,
        [this]()
        {
            updatePasswordPlaceholders();

            if (ui->confirmPasswordInputField->text().isEmpty() ==
                ui->passwordInputField->text().isEmpty())
            {
                ui->confirmPasswordInputField->validate();
            }
        });

    connect(ui->confirmPasswordInputField, &InputField::textChanged,
        this, &QnUserSettingsWidget::updatePasswordPlaceholders);

    connect(ui->passwordInputField, &InputField::editingFinished,
        ui->confirmPasswordInputField, &InputField::updateDisplayStateDelayed);

    ui->confirmPasswordInputField->setTitle(tr("Confirm Password"));
    ui->confirmPasswordInputField->setValidator(
        validatorsConcatenator(
        {
            defaultConfirmationValidator(
                [this]() { return ui->passwordInputField->text(); },
                tr("Passwords do not match.")),
            digestConfirmationValidator(m_model)
        }));

    for (auto field: inputFields())
        connect(field, &InputField::textChanged, this, &QnUserSettingsWidget::hasChangesChanged);
}

QList<InputField*> QnUserSettingsWidget::inputFields() const
{
    return m_localInputFields + m_cloudInputFields;
}

QList<InputField*> QnUserSettingsWidget::localInputFields() const
{
    return m_localInputFields;
}

QList<InputField*> QnUserSettingsWidget::cloudInputFields() const
{
    return m_cloudInputFields;
}

QList<InputField*> QnUserSettingsWidget::relevantInputFields() const
{
    if (m_model->mode() == QnUserSettingsModel::NewUser)
        return inputFields();

    if (m_model->mode() == QnUserSettingsModel::OtherSettings)
        return (m_model->user()->isCloud() ? cloudInputFields() : localInputFields());

    return {};
}

void QnUserSettingsWidget::updateRoleComboBox()
{
    if (!m_model->user())
        return;

    ui->roleComboBox->setCurrentIndex(m_rolesModel->rowForUser(m_model->user()));
}

void QnUserSettingsWidget::updateControlsAccess()
{
    Qn::Permissions permissions;
    if (m_model->mode() == QnUserSettingsModel::NewUser)
    {
        permissions = Qn::FullUserPermissions;
    }
    else
    {
        permissions = m_model->user()
            ? accessController()->permissions(m_model->user())
            : Qn::NoPermissions;
    }

    ui->loginInputField->setReadOnly(!permissions.testFlag(Qn::WriteNamePermission));
    ui->passwordInputField->setVisible(permissions.testFlag(Qn::WritePasswordPermission));
    ui->confirmPasswordInputField->setVisible(permissions.testFlag(Qn::WritePasswordPermission));
    ui->emailInputField->setReadOnly(!permissions.testFlag(Qn::WriteEmailPermission));
    ui->nameInputField->setReadOnly(!permissions.testFlag(Qn::WriteFullNamePermission));
}

Qn::UserRole QnUserSettingsWidget::selectedRole() const
{
    return ui->roleComboBox->itemData(ui->roleComboBox->currentIndex(), Qn::UserRoleRole).value<Qn::UserRole>();
}

QnUuid QnUserSettingsWidget::selectedUserRoleId() const
{
    return ui->roleComboBox->itemData(ui->roleComboBox->currentIndex(), Qn::UuidRole).value<QnUuid>();
}

bool QnUserSettingsWidget::validMode() const
{
    return m_model->mode() == QnUserSettingsModel::OtherSettings
        || m_model->mode() == QnUserSettingsModel::NewUser;
}

bool QnUserSettingsWidget::mustUpdatePassword() const
{
    return m_model->user() && ui->loginInputField->text() != m_model->user()->getName();
}

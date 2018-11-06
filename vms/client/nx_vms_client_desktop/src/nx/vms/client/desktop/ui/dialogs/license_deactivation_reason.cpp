#include "license_deactivation_reason.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/common/widgets/input_field.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/common/widgets/text_edit_field.h>
#include <nx/vms/client/desktop/common/widgets/combo_box_field.h>

#include <core/resource/user_resource.h>
#include <api/global_settings.h>
#include <common/common_module.h>

using namespace nx::vms::client::desktop;

namespace {

TextValidateFunction reasonComboBoxValidator(
    const QStringList& reasons,
    const QString& errorText)
{
    const auto validator =
        [reasons, errorText](const QString& text)
        {
            const auto reasonIndex = reasons.indexOf(text);
            return  reasonIndex > 0 //< First index is "Choose option"
                ? ValidationResult::kValid
                : ValidationResult(errorText);
        };

    return validator;
}

bool isLastSelectedOption(ComboBoxField* field)
{
    return field && field->currentIndex() == (field->items().size() - 1);
}

TextValidateFunction reasonTextEditValidator(
    TextEditField* field,
    const QString& errorText)
{
    const auto nonEmptyValidator = defaultNonEmptyValidator(errorText);
    const auto validator =
        [nonEmptyValidator, field](const QString& text)
        {
            return field->isVisible() ? nonEmptyValidator(text) : ValidationResult::kValid;
        };

    return validator;
}

} // unnamed namespace

namespace nx::vms::client::desktop {
namespace ui {
namespace dialogs {

LicenseDeactivationReason::LicenseDeactivationReason(
    const license::RequestInfo& info,
    QWidget* parent)
    :
    base_type(parent),
    m_info(info)
{
    setIcon(QnMessageBoxIcon::Information);
    setText(tr("Please fill up information about yourself and reason for license deactivation"));
    setStandardButtons(QDialogButtonBox::Cancel);
    setDefaultButton(QDialogButtonBox::NoButton);

    const auto nextButton = addButton(tr("Next"),
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

    setDefaultButton(nextButton);
    setEscapeButton(QDialogButtonBox::Cancel);

    addCustomWidget(createWidget(nextButton), Layout::Main, 0, Qt::Alignment(), true);
}

LicenseDeactivationReason::~LicenseDeactivationReason()
{
}

QWidget* LicenseDeactivationReason::createWidget(QPushButton* nextButton)
{
    auto widget = new QWidget();
    widget->setFocusPolicy(Qt::TabFocus);

    auto layout = new QVBoxLayout(widget);

    const auto addLabel = [layout](const QString& name) { layout->addWidget(new QLabel(name)); };

    const auto handleFieldChanges =
        [this, nextButton](detail::BaseInputField* field)
        {
            if (field->isValid())
                m_invalidFields.remove(field);
            else
                m_invalidFields.insert(field);

            nextButton->setEnabled(m_invalidFields.isEmpty());
        };

    const auto addWidget =
        [this, widget, layout, handleFieldChanges](detail::BaseInputField* field)
        {
            handleFieldChanges(field);
            connect(field, &detail::BaseInputField::isValidChanged, this,
                [field, handleFieldChanges]() { handleFieldChanges(field); });

            layout->addWidget(field);
        };

    const auto nameField = InputField::create(m_info.name,
        defaultNonEmptyValidator(tr("Name is necessary")));
    const auto emailField = InputField::create(m_info.email,
        defaultEmailValidator(false));

    static const auto kReasonNecessaryText = tr("Reason is necessary");
    const auto reasonComboBox = ComboBoxField::create(reasons(), 0,
        reasonComboBoxValidator(reasons(), kReasonNecessaryText), this);
    reasonComboBox->setRowHidden(0);
    const auto reasonField = TextEditField::create(QString(), TextValidateFunction());
    reasonField->setValidator(reasonTextEditValidator(reasonField, kReasonNecessaryText));

    const auto updateReasonFieldState =
        [reasonComboBox, handleFieldChanges, reasonField]()
        {
            const auto reasonIsVisible = isLastSelectedOption(reasonComboBox);
            reasonField->setVisible(reasonIsVisible);
            if (reasonIsVisible)
            {
                if (reasonField->calculateValidationResult().state == QValidator::Invalid)
                    reasonField->setIntermediateResult();
                reasonField->setFocus(Qt::ActiveWindowFocusReason);
            }
            handleFieldChanges(reasonField);
        };


    connect(reasonComboBox, &ComboBoxField::currentIndexChanged, this, updateReasonFieldState);
    updateReasonFieldState();

    connect(nextButton, &QPushButton::clicked, this,
        [this, nameField, emailField, reasonComboBox, reasonField]()
        {
            const auto reasonText = isLastSelectedOption(reasonComboBox)
                ? reasonField->text().split(lit("\n"))
                : QStringList(reasonComboBox->text());

            const auto systemName = qnGlobalSettings->systemName();
            const auto userName = context()->user()->getName();
            m_info = nx::vms::client::desktop::license::RequestInfo({
                nameField->text(), emailField->text(), reasonText, systemName, userName});
        });

    addLabel(tr("Name"));
    addWidget(nameField);
    addLabel(tr("Email"));
    addWidget(emailField);
    addLabel(tr("Reason for deactivation"));
    addWidget(reasonComboBox);
    addWidget(reasonField);

    widget->setFocusProxy(nameField);

    return widget;
}

QStringList LicenseDeactivationReason::reasons()
{
    static const QStringList kReasons{
        tr("- Choose one -"),
        tr("I am upgrading my machine"),
        tr("I accidentally assigned the license to a wrong machine"),
        tr("Other Reason")};

    return kReasons;
}

license::RequestInfo LicenseDeactivationReason::info()
{
    return m_info;
}

} // namespace dialogs
} // namespace ui
} // namespace nx::vms::client::desktop


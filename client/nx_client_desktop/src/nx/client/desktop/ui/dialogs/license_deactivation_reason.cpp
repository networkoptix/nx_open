#include "license_deactivation_reason.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>

#include <ui/widgets/common/input_field.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/ui/common/text_edit_field.h>
#include <nx/client/desktop/ui/common/combo_box_field.h>

#include <core/resource/user_resource.h>
#include <api/global_settings.h>
#include <common/common_module.h>

namespace {

using namespace nx::client::desktop::ui;

Qn::TextValidateFunction reasonComboBoxValidator(
    const QStringList& reasons,
    const QString& errorText)
{
    const auto validator =
        [reasons, errorText](const QString& text)
        {
            const auto reasonIndex = reasons.indexOf(text);
            return  reasonIndex > 0 //< First index is "Choose option"
                ? Qn::kValidResult
                : Qn::ValidationResult(errorText);
        };

    return validator;
}

bool isLastSelectedOption(ComboBoxField* field)
{
    return field && field->currentIndex() == (field->items().size() - 1);
}

Qn::TextValidateFunction reasonTextEditValidator(
    TextEditField* field,
    const QString& errorText)
{
    const auto nonEmptyValidator = Qn::defaultNonEmptyValidator(errorText);
    const auto validator =
        [nonEmptyValidator, field](const QString& text)
        {
            return field->isVisible() ? nonEmptyValidator(text) : Qn::kValidResult;
        };

    return validator;
}

} // unnamed namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

using namespace nx::client::desktop::ui;

LicenseDeactivationReason::LicenseDeactivationReason(
    const license::RequestInfo& info,
    QWidget* parent)
    :
    base_type(QnMessageBoxIcon::Information,
        tr("Please fill up information about yourself and reason for license deactivation"),
        QString(), QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parent),
    QnWorkbenchContextAware(parent),
    m_info(info)
{
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

    const auto nameField = QnInputField::create(m_info.name,
        Qn::defaultNonEmptyValidator(tr("Name is necessary")));
    const auto emailField = QnInputField::create(m_info.email,
        Qn::defaultEmailValidator(false));

    static const auto kReasonNecessaryText = tr("Reason is necessary");
    const auto reasonComboBox = ComboBoxField::create(reasons(), 0,
        reasonComboBoxValidator(reasons(), kReasonNecessaryText), this);
    reasonComboBox->setRowHidden(0);
    const auto reasonField = TextEditField::create(QString(), Qn::TextValidateFunction());
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
            m_info = nx::client::desktop::license::RequestInfo({
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
} // namespace desktop
} // namespace client
} // namespace nx


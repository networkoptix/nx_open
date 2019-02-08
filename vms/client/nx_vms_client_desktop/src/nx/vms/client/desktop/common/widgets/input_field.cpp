#include "input_field.h"
#include "password_strength_indicator.h"

#include <QtGui/QFocusEvent>

#include <QtWidgets/QBoxLayout>

#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <nx/vms/client/desktop/common/widgets/password_preview_button.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/common/delayed.h>


namespace nx::vms::client::desktop {

namespace {

detail::BaseInputField::AccessorPtr accessor(const QByteArray& propertyName)
{
    return detail::BaseInputField::AccessorPtr(new PropertyAccessor(propertyName));
}

} // namespace

struct InputField::Private
{
public:
    explicit Private(InputField* q): q(q)
    {
    }

    void updatePasswordIndicatorVisibility()
    {
        if (!passwordIndicator)
            return;

        if (!q->text().trimmed().isEmpty())
            passwordIndicator->setVisible(true);
        else if (hidePasswordIndicatorWhenEmpty)
            passwordIndicator->setVisible(false);
    }

    InputField* const q = nullptr;
    PasswordStrengthIndicator* passwordIndicator = nullptr;
    bool hidePasswordIndicatorWhenEmpty = true;
};

//-------------------------------------------------------------------------------------------------

InputField* InputField::create(
    const QString& text,
    const TextValidateFunction& validator,
    QWidget* parent)
{
    const auto result = new InputField(parent);
    result->setValidator(validator);
    result->setText(text);
    return result;
}

InputField::InputField(QWidget* parent /*= nullptr*/) :
    base_type(new QLineEdit(), accessor("text"),
        accessor("readOnly"), accessor("placeholderText"), true, parent),
    d(new Private(this))
{
    const auto lineEdit = qobject_cast<QLineEdit*>(input());
    connect(lineEdit, &QLineEdit::editingFinished, this, &InputField::editingFinished);
    connect(lineEdit, &QLineEdit::textChanged, this,
        [this]()
        {
            d->updatePasswordIndicatorVisibility();
            emit textChanged(text());
        });
}

InputField::~InputField()
{
}

QLineEdit::EchoMode InputField::echoMode() const
{
    return lineEdit()->echoMode();
}

void InputField::setEchoMode(QLineEdit::EchoMode value)
{
    lineEdit()->setEchoMode(value);
}

QString InputField::inputMask() const
{
    return lineEdit()->inputMask();
}

void InputField::setInputMask(const QString& inputMask)
{
    lineEdit()->setInputMask(inputMask);
}

const PasswordStrengthIndicator* InputField::passwordIndicator() const
{
    return d->passwordIndicator;
}

bool InputField::passwordIndicatorEnabled() const
{
    return passwordIndicator() != nullptr;
}

void InputField::setPasswordIndicatorEnabled(
    bool enabled,
    bool hideForEmptyInput,
    bool showImmediately,
    PasswordInformation::AnalyzeFunction analyzeFunction)
{
    bool update = d->hidePasswordIndicatorWhenEmpty != hideForEmptyInput;
    d->hidePasswordIndicatorWhenEmpty = hideForEmptyInput;

    if (enabled)
    {
        if (!d->passwordIndicator)
        {
            d->passwordIndicator = new PasswordStrengthIndicator(lineEdit(), analyzeFunction);
            d->passwordIndicator->setVisible(false);
        }

        if (update || showImmediately)
            d->updatePasswordIndicatorVisibility();
    }
    else
    {
        delete d->passwordIndicator;
        d->passwordIndicator = nullptr;
    }
}

void InputField::useForPassword()
{
    setEchoMode(QLineEdit::Password);
    PasswordPreviewButton::createInline(lineEdit());
}

QLineEdit* InputField::lineEdit()
{
    return dynamic_cast<QLineEdit*>(input());
}

const QLineEdit* InputField::lineEdit() const
{
    return dynamic_cast<const QLineEdit*>(input());
}

ValidationResult InputField::calculateValidationResult() const
{
    auto result = base_type::calculateValidationResult();
    if ((result.state == QValidator::Acceptable)
        && d->passwordIndicator && d->passwordIndicator->isVisible())
    {
        const auto& info = d->passwordIndicator->currentInformation();
        if (info.acceptance() == utils::PasswordAcceptance::Unacceptable)
            result = ValidationResult(info.hint());
    }

    return result;
}

} // namespace nx::vms::client::desktop

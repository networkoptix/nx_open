#include "input_field.h"
#include "password_strength_indicator.h"

#include <QtGui/QFocusEvent>

#include <QtWidgets/QBoxLayout>

#include <nx/client/desktop/common/utils/accessor.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/common/delayed.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

QLineEdit* toLineEdit(QWidget* widget)
{
    return qobject_cast<QLineEdit*>(widget);
}

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
    return toLineEdit(input())->echoMode();
}

void InputField::setEchoMode(QLineEdit::EchoMode value)
{
    toLineEdit(input())->setEchoMode(value);
}

QString InputField::inputMask() const
{
    return toLineEdit(input())->inputMask();
}

void InputField::setInputMask(const QString& inputMask)
{
    return toLineEdit(input())->setInputMask(inputMask);
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
            d->passwordIndicator = new PasswordStrengthIndicator(
                toLineEdit(input()), analyzeFunction);
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

ValidationResult InputField::calculateValidationResult() const
{
    auto result = base_type::calculateValidationResult();
    if ((result.state == QValidator::Acceptable)
        && d->passwordIndicator && d->passwordIndicator->isVisible())
    {
        const auto& info = d->passwordIndicator->currentInformation();
        if (info.acceptance() == PasswordInformation::Inacceptable)
            result = ValidationResult(info.hint());
    }

    return result;
}

} // namespace desktop
} // namespace client
} // namespace nx

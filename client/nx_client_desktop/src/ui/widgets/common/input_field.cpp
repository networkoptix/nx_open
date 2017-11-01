#include "input_field.h"
#include "password_strength_indicator.h"

#include <QtGui/QFocusEvent>

#include <QtWidgets/QBoxLayout>

#include <ui/common/accessor.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/common/delayed.h>

namespace {

using namespace nx::client::desktop::ui::detail;

QLineEdit* toLineEdit(QWidget* widget)
{
    return qobject_cast<QLineEdit*>(widget);
}

BaseInputField::AccessorPtr accessor(const QByteArray& propertyName)
{
    return BaseInputField::AccessorPtr(new PropertyAccessor(propertyName));
}

} // namespace

class QnInputFieldPrivate : public QObject
{
public:
    QnInputFieldPrivate(QnInputField* parent) :
        QObject(parent),
        parent(parent),
        passwordIndicator(nullptr),
        hidePasswordIndicatorWhenEmpty(true)
    {
    }

    void updatePasswordIndicatorVisibility()
    {
        if (!passwordIndicator)
            return;

        if (!parent->text().trimmed().isEmpty())
            passwordIndicator->setVisible(true);
        else if (hidePasswordIndicatorWhenEmpty)
            passwordIndicator->setVisible(false);
    }

    QnInputField* parent;
    QLabel* title;
    QnWordWrappedLabel* hint;

    QnPasswordStrengthIndicator* passwordIndicator;
    bool hidePasswordIndicatorWhenEmpty;
};

//-------------------------------------------------------------------------------------------------

QnInputField* QnInputField::create(
    const QString& text,
    const Qn::TextValidateFunction& validator,
    QWidget* parent)
{
    const auto result = new QnInputField(parent);
    result->setValidator(validator);
    result->setText(text);
    return result;
}

QnInputField::QnInputField(QWidget* parent /*= nullptr*/) :
    base_type(new QLineEdit(), accessor("text"),
        accessor("readOnly"), accessor("placeholderText"), true, parent),
    d_ptr(new QnInputFieldPrivate(this))
{
    const auto lineEdit = qobject_cast<QLineEdit*>(input());
    connect(lineEdit, &QLineEdit::editingFinished, this, &QnInputField::editingFinished);
    connect(lineEdit, &QLineEdit::textChanged, this,
        [this]()
        {
            Q_D(QnInputField);
            d->updatePasswordIndicatorVisibility();

            emit textChanged(text());
        });
}

QnInputField::~QnInputField()
{
}

QLineEdit::EchoMode QnInputField::echoMode() const
{
    return toLineEdit(input())->echoMode();
}

void QnInputField::setEchoMode(QLineEdit::EchoMode value)
{
    toLineEdit(input())->setEchoMode(value);
}

QString QnInputField::inputMask() const
{
    return toLineEdit(input())->inputMask();
}

void QnInputField::setInputMask(const QString& inputMask)
{
    return toLineEdit(input())->setInputMask(inputMask);
}

const QnPasswordStrengthIndicator* QnInputField::passwordIndicator() const
{
    Q_D(const QnInputField);
    return d->passwordIndicator;
}

bool QnInputField::passwordIndicatorEnabled() const
{
    return passwordIndicator() != nullptr;
}

void QnInputField::setPasswordIndicatorEnabled(
    bool enabled,
    bool hideForEmptyInput,
    bool showImmediately,
    QnPasswordInformation::AnalyzeFunction analyzeFunction)
{
    Q_D(QnInputField);

    bool update = d->hidePasswordIndicatorWhenEmpty != hideForEmptyInput;
    d->hidePasswordIndicatorWhenEmpty = hideForEmptyInput;

    if (enabled)
    {
        if (!d->passwordIndicator)
        {
            d->passwordIndicator = new QnPasswordStrengthIndicator(
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

Qn::ValidationResult QnInputField::calculateValidationResult() const
{
    Q_D(const QnInputField);

    auto result = base_type::calculateValidationResult();
    if ((result.state == QValidator::Acceptable)
        && d->passwordIndicator && d->passwordIndicator->isVisible())
    {
        const auto& info = d->passwordIndicator->currentInformation();
        if (info.acceptance() == QnPasswordInformation::Inacceptable)
            result = Qn::ValidationResult(info.hint());
    }

    return result;
}

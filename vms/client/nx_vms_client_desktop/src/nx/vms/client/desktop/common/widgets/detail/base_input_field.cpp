#include "base_input_field.h"

#include <QtGui/QFocusEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>

#include <ui/style/custom_style.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/common/delayed.h>

#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {
namespace detail {

class BaseInputFieldPrivate: public QObject
{
public:
    BaseInputFieldPrivate(
        QWidget* inputInstance,
        const BaseInputField::AccessorPtr& textAccessor,
        const BaseInputField::AccessorPtr& readOnlyAccessor,
        const BaseInputField::AccessorPtr& placeholderAccessor,
        bool useWarningStyleForControl,
        BaseInputField* parent);

    virtual bool eventFilter(QObject* watched, QEvent* event);

    void updateDisplayStateDelayed();
    void setHintText(const QString& text);

    void setText(const QString& text);
    QString getText() const;

    ValidationResult getLastResult() const;
    void setLastResult(ValidationResult result);
    void setAcceptableResult();
    void setIntermediateResult();

    void updateStateWhileActive();
    void updateVisualStateDelayed();

    void updatePlaceholder()
    {
        if (!placeholderAccessor || !input)
            return;

        placeholderAccessor->set(input, optionalTextIsNull
            ? optionalTextPlaceholder
            : normalTextPlaceholder);
    }

    void initializeControls();

public:
    BaseInputField* parent = nullptr;
    QLabel* title = nullptr;
    QnWordWrappedLabel* hint = nullptr;
    QString customHintText;
    QWidget* input = nullptr;
    QPalette defaultPalette;

    TextValidateFunction validator;
    bool useWarningStyleForControl = false;

    bool optionalTextIsNull = false;
    bool settingOptionalText = false;
    QString normalTextPlaceholder;
    QString optionalTextPlaceholder;

    const BaseInputField::AccessorPtr textAccessor;
    const BaseInputField::AccessorPtr readOnlyAccessor;
    const BaseInputField::AccessorPtr placeholderAccessor;
    bool externalControls = false;
    QColor hintColor;

private:
    ValidationResult lastResult;
};

BaseInputFieldPrivate::BaseInputFieldPrivate(
    QWidget* inputInstance,
    const BaseInputField::AccessorPtr& textAccessor,
    const BaseInputField::AccessorPtr& readOnlyAccessor,
    const BaseInputField::AccessorPtr& placeholderAccessor,
    bool useWarningStyleForControl,
    BaseInputField* parent)
    :
    QObject(parent),
    parent(parent),
    title(new QLabel(parent)),
    hint(new QnWordWrappedLabel(parent)),
    input(inputInstance),
    defaultPalette(),
    useWarningStyleForControl(useWarningStyleForControl),
    optionalTextPlaceholder(lit("<%1>").arg(BaseInputField::tr("multiple values"))),
    textAccessor(textAccessor),
    readOnlyAccessor(readOnlyAccessor),
    placeholderAccessor(placeholderAccessor),
    lastResult(QValidator::Intermediate)
{
    initializeControls();
}

void BaseInputFieldPrivate::initializeControls()
{
    hint->setOpenExternalLinks(true);
    input->installEventFilter(this);
    parent->setFocusProxy(input);
}

void BaseInputFieldPrivate::updateStateWhileActive()
{
    if (!parent->hasFocus())
        return;

    if (parent->calculateValidationResult().state == QValidator::Invalid)
        parent->setIntermediateResult();
    else
        parent->validate();
}

ValidationResult BaseInputFieldPrivate::getLastResult() const
{
    return lastResult;
}

void BaseInputFieldPrivate::updateVisualStateDelayed()
{
    const auto updateFunction =
        [this]()
        {
            setHintText(customHintText.isEmpty() ? lastResult.errorMessage : customHintText);

            QPalette palette = defaultPalette;
            if (lastResult.state == QValidator::Invalid)
                setWarningStyle(&palette);

            if (useWarningStyleForControl)
                input->setPalette(palette);

            if (hintColor.isValid())
                setCustomStyle(&palette, hintColor);

            hint->setPalette(customHintText.isEmpty() ? palette : defaultPalette);
        };
    executeDelayedParented(updateFunction, 0, this);
}

void BaseInputFieldPrivate::setLastResult(ValidationResult result)
{
    if (lastResult.state == result.state)
        return;

    lastResult = result;

    updateVisualStateDelayed();
    emit parent->isValidChanged();
}

void BaseInputFieldPrivate::setAcceptableResult()
{
    setLastResult(ValidationResult(QValidator::Acceptable));
}

void BaseInputFieldPrivate::setIntermediateResult()
{
    setLastResult(ValidationResult(QValidator::Intermediate));
}

bool BaseInputFieldPrivate::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != input)
        return false;

    switch (event->type())
    {
        case QEvent::Polish:
        {
            // Ensure input is polished.
            const auto callback = [this](){ defaultPalette = input->palette(); };
            executeLater(nx::utils::guarded(this, callback), this);
            break;
        }
        case QEvent::FocusIn:
        {
            updateStateWhileActive();
            break;
        }
        case QEvent::FocusOut:
        {
            // On focus loss perform validate, unless it's a popup focus change.
            switch (static_cast<QFocusEvent*>(event)->reason())
            {
                case Qt::ActiveWindowFocusReason:
                case Qt::MenuBarFocusReason:
                case Qt::OtherFocusReason:
                    break;

                default:
                    updateDisplayStateDelayed();
                    break;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

void BaseInputFieldPrivate::updateDisplayStateDelayed()
{
    const auto handler =
        [this]()
        {
            if (!input->hasFocus())
                parent->validate();
        };

    const int kValidateDelayMs = 150;
    executeDelayedParented(handler, kValidateDelayMs, this);
}

void BaseInputFieldPrivate::setHintText(const QString& text)
{
    if (hint->text() == text)
        return;

    hint->setText(text);
    hint->setVisible(!text.isEmpty());

    for (QWidget* widget = parent; widget && widget->layout(); widget = widget->parentWidget())
      widget->layout()->activate();
}

void BaseInputFieldPrivate::setText(const QString& text)
{
    if (input && textAccessor)
        textAccessor->set(input, text);
}

QString BaseInputFieldPrivate::getText() const
{
    return input && textAccessor
        ? textAccessor->get(input).toString()
        : QString();
}

//-------------------------------------------------------------------------------------------------

class LabelWidthAccessor: public AbstractAccessor
{
public:
    virtual QVariant get(const QObject* object) const override;
    virtual void set(QObject* object, const QVariant& value) const override;
};

QVariant LabelWidthAccessor::get(const QObject* object) const
{
    const auto inputField = qobject_cast<const BaseInputField*>(object);
    return inputField ? inputField->d_ptr->title->sizeHint().width() : 0;
}

void LabelWidthAccessor::set(QObject* object, const QVariant& value) const
{
    if (auto inputField = qobject_cast<BaseInputField*>(object))
        inputField->d_ptr->title->setFixedWidth(value.toInt());
}

//-------------------------------------------------------------------------------------------------

BaseInputField::BaseInputField(
    QWidget* inputInstance,
    const AccessorPtr& textAccessor,
    const AccessorPtr& readOnlyAccessor,
    const AccessorPtr& placeholderAccessor,
    bool useWarningStyleForControl,
    QWidget* parent)
    :
    base_type(parent),
    d_ptr(new BaseInputFieldPrivate(inputInstance, textAccessor,
        readOnlyAccessor, placeholderAccessor, useWarningStyleForControl, this))
{
    Q_D(BaseInputField);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);

    QGridLayout* grid = new QGridLayout(this);
    grid->addWidget(d->title, 0, 0, 1, 1, Qt::AlignRight);
    grid->addWidget(d->input, 0, 1, 1, 1);
    grid->addWidget(d->hint,  1, 1, 1, 1);
    grid->setMargin(0);

    d->title->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    d->title->setVisible(false);
    d->hint->setVisible(false);

    connect(this, &BaseInputField::textChanged, d,
        [d]()
        {
            if (!d->settingOptionalText)
                d->optionalTextIsNull = false;

            d->updatePlaceholder();
            d->updateStateWhileActive();
        });
}

BaseInputField::~BaseInputField()
{
}

void BaseInputField::setExternalControls(
    QLabel* titleLabel,
    QnWordWrappedLabel* hintLabel)
{
    Q_D(BaseInputField);

    if (!d->externalControls)
    {
        delete d->title;
        delete d->hint;
    }

    d->externalControls = true;
    d->title = titleLabel;
    d->hint = hintLabel;

    d->initializeControls();
}

QWidget* BaseInputField::input() const
{
    Q_D(const BaseInputField);
    return d->input;
}

QString BaseInputField::title() const
{
    Q_D(const BaseInputField);
    return d->title->text();
}

void BaseInputField::setTitle(const QString& value)
{
    Q_D(BaseInputField);
    d->title->setText(value);
    d->title->setVisible(!value.isEmpty());
}

void BaseInputField::setCustomHint(const QString& hint)
{
    Q_D(BaseInputField);
    d->customHintText = hint;
    d->updateVisualStateDelayed();
}

QString BaseInputField::text() const
{
    Q_D(const BaseInputField);
    return d->textAccessor->get(d->input).toString();
}

void BaseInputField::setText(const QString& value)
{
    Q_D(BaseInputField);
    if (d->textAccessor->get(d->input) == value)
        return;

    d->textAccessor->set(d->input, value);
    validate();
}

QString BaseInputField::placeholderText() const
{
    Q_D(const BaseInputField);
    return d->normalTextPlaceholder;
}

void BaseInputField::setPlaceholderText(const QString& value)
{
    Q_D(BaseInputField);
    if (d->normalTextPlaceholder == value)
        return;

    d->normalTextPlaceholder = value;
    d->updatePlaceholder();
}

std::optional<QString> BaseInputField::optionalText() const
{
    Q_D(const BaseInputField);
    return d->optionalTextIsNull ? std::optional<QString>() : text();
}

void BaseInputField::setOptionalText(const std::optional<QString>& value)
{
    Q_D(BaseInputField);

    d->optionalTextIsNull = !value;
    d->updatePlaceholder();

    auto updateGuard = nx::utils::makeScopeGuard(
        [d]() { d->settingOptionalText = false; });
    d->settingOptionalText = true;

    setText(value.value_or(QString()));
}

QString BaseInputField::optionalTextPlaceholder() const
{
    Q_D(const BaseInputField);
    return d->optionalTextPlaceholder;
}

void BaseInputField::setOptionalTextPlaceholder(const QString& value)
{
    Q_D(BaseInputField);
    if (d->optionalTextPlaceholder == value)
        return;

    d->optionalTextPlaceholder = value;
    d->updatePlaceholder();
}

bool BaseInputField::isReadOnly() const
{
    Q_D(const BaseInputField);
    return d->readOnlyAccessor && d->input && d->readOnlyAccessor->get(d->input).toBool();
}

void BaseInputField::setReadOnly(bool value)
{
    Q_D(BaseInputField);
    if (d->input &&d->readOnlyAccessor)
        d->readOnlyAccessor->set(d->input, value);
}

bool BaseInputField::validate()
{
    Q_D(BaseInputField);

    d->input->ensurePolished();
    const auto result = calculateValidationResult();

    d->setLastResult(result);
    return lastValidationResult() != QValidator::Invalid;
}

void BaseInputField::updateDisplayStateDelayed()
{
    Q_D(BaseInputField);
    d->updateDisplayStateDelayed();
}

void BaseInputField::clear()
{
    Q_D(BaseInputField);
    d->textAccessor->set(d->input, QString());
    validate();
}

bool BaseInputField::isValid() const
{
    return calculateValidationResult().state == QValidator::Acceptable;
}

void BaseInputField::setHintColor(const QColor& color)
{
    Q_D(BaseInputField);
    d->hintColor = color;
}

QValidator::State BaseInputField::lastValidationResult() const
{
    Q_D(const BaseInputField);
    return d->getLastResult().state;
}

void BaseInputField::setValidator(
    TextValidateFunction validator,
    bool validateImmediately)
{
    Q_D(BaseInputField);
    d->validator = validator;

    if (validateImmediately)
        validate();
}

void BaseInputField::reset()
{
    Q_D(BaseInputField);
    d->setAcceptableResult();
    d->setHintText(QString());
}

AbstractAccessor* BaseInputField::createLabelWidthAccessor()
{
    return new LabelWidthAccessor();
}

void BaseInputField::setIntermediateResult()
{
    Q_D(BaseInputField);
    d->setIntermediateResult();
}

ValidationResult BaseInputField::calculateValidationResult() const
{
    Q_D(const BaseInputField);
    return d->validator && !d->optionalTextIsNull
        ? d->validator(d->getText())
        : ValidationResult(QValidator::Acceptable);
}

} // namespace detail
} // namespace nx::vms::client::desktop

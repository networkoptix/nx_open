#include "base_input_field.h"

#include <QtGui/QFocusEvent>

#include <QtWidgets/QBoxLayout>

#include <ui/common/accessor.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/common/delayed.h>

#include <QApplication>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace detail {

class BaseInputFieldPrivate: public QObject
{
public:
    BaseInputFieldPrivate(
        QWidget* inputInstance,
        const BaseInputField::AccessorPtr& textAccessor,
        const BaseInputField::AccessorPtr& readOnlyAccessor,
        const BaseInputField::AccessorPtr& placeholderAccessor,
        BaseInputField* parent);

    virtual bool eventFilter(QObject* watched, QEvent* event);

    void updateDisplayStateDelayed();
    void setHintText(const QString& text);

    void setText(const QString& text);
    QString getText() const;

    Qn::ValidationResult getLastResult() const;
    void setLastResult(Qn::ValidationResult result);
    void clearValidationResult();

public:
    BaseInputField* parent;
    QLabel* title;
    QnWordWrappedLabel* hint;
    QWidget* input;
    QPalette defaultPalette;

    Qn::TextValidateFunction validator;

    const BaseInputField::AccessorPtr textAccessor;
    const BaseInputField::AccessorPtr readOnlyAccessor;
    const BaseInputField::AccessorPtr placeholderAccessor;

private:
    void updatePalette();

private:
    Qn::ValidationResult lastResult;
};

BaseInputFieldPrivate::BaseInputFieldPrivate(
    QWidget* inputInstance,
    const BaseInputField::AccessorPtr& textAccessor,
    const BaseInputField::AccessorPtr& readOnlyAccessor,
    const BaseInputField::AccessorPtr& placeholderAccessor,
    BaseInputField* parent)
    :
    QObject(parent),
    parent(parent),
    title(new QLabel(parent)),
    hint(new QnWordWrappedLabel(parent)),
    input(inputInstance),
    defaultPalette(),    

    textAccessor(textAccessor),
    readOnlyAccessor(readOnlyAccessor),
    placeholderAccessor(placeholderAccessor),

    lastResult(QValidator::Acceptable)
{
    input->installEventFilter(this);
    parent->setFocusProxy(input);
}

Qn::ValidationResult BaseInputFieldPrivate::getLastResult() const
{
    return lastResult;
}

void BaseInputFieldPrivate::updatePalette()
{
    setHintText(lastResult.errorMessage);

    QPalette palette = defaultPalette;
    if (lastResult.state != QValidator::Acceptable)
        setWarningStyle(&palette);

    if (!input->hasFocus())
        input->setPalette(palette);

    hint->setPalette(palette);
}

void BaseInputFieldPrivate::setLastResult(Qn::ValidationResult result)
{
    if (lastResult.state == result.state)
        return;

    lastResult = result;

    updatePalette();
    emit parent->isValidChanged();
}

void BaseInputFieldPrivate::clearValidationResult()
{
    setLastResult(Qn::ValidationResult(QValidator::Acceptable));
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
            executeDelayed([this](){ defaultPalette = input->palette(); }, 0);
            break;
        }
        case QEvent::FocusIn:
        {
            // On focus gain make input look usual even if there is error.
            setHintText(QString());
            input->setPalette(defaultPalette);
            lastResult.state = QValidator::Intermediate;
            break;
        }
        case QEvent::FocusOut:
        {
            // On focus loss perform validate, unless it's a popup or programmable focus change.
            switch (static_cast<QFocusEvent*>(event)->reason())
            {
                case Qt::ActiveWindowFocusReason:
                case Qt::MenuBarFocusReason:
                case Qt::PopupFocusReason:
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
    QWidget* parent)
    :
    base_type(parent),
    d_ptr(new BaseInputFieldPrivate(inputInstance, textAccessor,
        readOnlyAccessor, placeholderAccessor, this))
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
}

BaseInputField::~BaseInputField()
{
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

QString BaseInputField::hint() const
{
    Q_D(const BaseInputField);
    return d->hint->text();
}

void BaseInputField::setHint(const QString& value)
{
    Q_D(BaseInputField);
    d->setHintText(value);
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
    return d->placeholderAccessor->get(d->input).toString();
}

void BaseInputField::setPlaceholderText(const QString& value)
{
    Q_D(BaseInputField);
    d->placeholderAccessor->set(d->input, value);
}

bool BaseInputField::isReadOnly() const
{
    Q_D(const BaseInputField);
    return d->readOnlyAccessor->get(d->input).toBool();
}

void BaseInputField::setReadOnly(bool value)
{
    Q_D(BaseInputField);
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

QValidator::State BaseInputField::lastValidationResult() const
{
    Q_D(const BaseInputField);
    return d->getLastResult().state;
}

void BaseInputField::setValidator(
    Qn::TextValidateFunction validator,
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
    d->clearValidationResult();
    d->setHintText(QString());
}

AbstractAccessor* BaseInputField::createLabelWidthAccessor()
{
    return new LabelWidthAccessor();
}

Qn::ValidationResult BaseInputField::calculateValidationResult() const
{
    Q_D(const BaseInputField);
    return d->validator
        ? d->validator(d->getText())
        : Qn::ValidationResult(QValidator::Acceptable);
}

} // namespace detail
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

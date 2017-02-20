#include "input_field.h"
#include "password_strength_indicator.h"

#include <ui/common/accessor.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/common/delayed.h>

class QnInputFieldPrivate : public QObject
{
public:
    QnInputFieldPrivate(QWidget* parent) :
        QObject(parent),
        parent(parent),
        title(new QLabel(parent)),
        hint(new QnWordWrappedLabel(parent)),
        input(new QLineEdit(parent)),
        lastValidationResult(QValidator::Acceptable),
        validator(),
        passwordIndicator(nullptr),
        hidePasswordIndicatorWhenEmpty(true)
    {
        input->installEventFilter(this);
        parent->setFocusProxy(input);

        connect(input, &QLineEdit::textChanged, this, &QnInputFieldPrivate::updatePasswordIndicatorVisibility);
    }

    virtual bool eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == input)
        {
            switch (event->type())
            {
                /* On focus gain make input look usual even if there is error. */
                case QEvent::FocusIn:
                {
                    setHintText(QString());
                    input->setPalette(parent->palette());
                    lastValidationResult.state = QValidator::Intermediate;
                    break;
                }

                /* On focus loss perform validate, unless it's a popup or programmable focus change. */
                case QEvent::FocusOut:
                {
                    switch (static_cast<QFocusEvent*>(event)->reason())
                    {
                        case Qt::ActiveWindowFocusReason:
                        case Qt::MenuBarFocusReason:
                        case Qt::PopupFocusReason:
                        case Qt::OtherFocusReason:
                            break;

                        default:
                            updateDisplayState();
                            break;
                    }
                    break;
                }

                default:
                    break;
            }
        }

        /* Always pass event further */
        return false;
    }

    void updateDisplayState()
    {
        const auto handler =
            [this]()
            {
                if (!input->hasFocus())
                    validate();
            };

        const int kValidateDelayMs = 150;
        executeDelayedParented(handler, kValidateDelayMs, this);
    }

    void setHintText(const QString& text)
    {
        if (hint->text() == text)
            return;

        hint->setText(text);
        hint->setVisible(!text.isEmpty());

        for (QWidget* widget = parent; widget && widget->layout(); widget = widget->parentWidget())
          widget->layout()->activate();
    }

    Qn::ValidationResult validationResult() const
    {
        Qn::ValidationResult validationResult(QValidator::Acceptable);
        if (validator)
            validationResult = validator(input->text());

        if ((validationResult.state == QValidator::Acceptable)
            && passwordIndicator && passwordIndicator->isVisible())
        {
            const auto& info = passwordIndicator->currentInformation();
            if (info.acceptance() == QnPasswordInformation::Inacceptable)
                validationResult = Qn::ValidationResult(info.hint());
        }

        return validationResult;
    }

    void validate()
    {
        clearValidationResult();
        updatePasswordIndicatorVisibility();
        lastValidationResult = validationResult();
        setHintText(lastValidationResult.errorMessage);

        QPalette palette = parent->palette();
        if (lastValidationResult.state != QValidator::Acceptable)
            setWarningStyle(&palette);

        if (!input->hasFocus())
            input->setPalette(palette);

        hint->setPalette(palette);
    }

    void clearValidationResult()
    {
        lastValidationResult = Qn::ValidationResult(QValidator::Acceptable);
    }

    void updatePasswordIndicatorVisibility()
    {
        if (passwordIndicator)
        {
            if (input->text().trimmed().isEmpty())
            {
                if (hidePasswordIndicatorWhenEmpty)
                    passwordIndicator->setVisible(false);
            }
            else
            {
                passwordIndicator->setVisible(true);
            }
        }
    }

    QWidget* parent;
    QLabel* title;
    QnWordWrappedLabel* hint;
    QLineEdit* input;

    Qn::ValidationResult lastValidationResult;
    Qn::TextValidateFunction validator;

    QnPasswordStrengthIndicator* passwordIndicator;
    bool hidePasswordIndicatorWhenEmpty;
};

class LabelWidthAccessor : public AbstractAccessor
{
public:
    LabelWidthAccessor() {}

    virtual QVariant get(const QObject* object) const override
    {
        const QnInputField* w = qobject_cast<const QnInputField*>(object);
        if (!w)
            return 0;
        return w->d_ptr->title->sizeHint().width();
    }

    virtual void set(QObject* object, const QVariant& value) const override
    {
        QnInputField* w = qobject_cast<QnInputField*>(object);
        if (!w)
            return;
        w->d_ptr->title->setFixedWidth(value.toInt());
    }
};

QnInputField::QnInputField(QWidget* parent /*= nullptr*/) :
    base_type(parent),
    d_ptr(new QnInputFieldPrivate(this))
{
    Q_D(QnInputField);
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

    connect(d->input, &QLineEdit::textChanged, this, &QnInputField::textChanged);
    connect(d->input, &QLineEdit::editingFinished, this, &QnInputField::editingFinished);
}

QnInputField::~QnInputField()
{
}

QString QnInputField::title() const
{
    Q_D(const QnInputField);
    return d->title->text();
}

void QnInputField::setTitle(const QString& value)
{
    Q_D(QnInputField);
    d->title->setText(value);
    d->title->setVisible(!value.isEmpty());
}

QString QnInputField::hint() const
{
    Q_D(const QnInputField);
    return d->hint->text();
}

void QnInputField::setHint(const QString& value)
{
    Q_D(QnInputField);
    d->setHintText(value);
}

QString QnInputField::text() const
{
    Q_D(const QnInputField);
    return d->input->text();
}

void QnInputField::setText(const QString& value)
{
    Q_D(QnInputField);
    if (d->input->text() == value)
        return;

    d->input->setText(value);
    d->validate();
}

QString QnInputField::placeholderText() const
{
    Q_D(const QnInputField);
    return d->input->placeholderText();
}

void QnInputField::setPlaceholderText(const QString& value)
{
    Q_D(QnInputField);
    d->input->setPlaceholderText(value);
}

QLineEdit::EchoMode QnInputField::echoMode() const
{
    Q_D(const QnInputField);
    return d->input->echoMode();
}

void QnInputField::setEchoMode(QLineEdit::EchoMode value)
{
    Q_D(QnInputField);
    d->input->setEchoMode(value);
}

QString QnInputField::inputMask() const
{
    Q_D(const QnInputField);
    return d->input->inputMask();
}

void QnInputField::setInputMask(const QString& inputMask)
{
    Q_D(QnInputField);
    return d->input->setInputMask(inputMask);
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

void QnInputField::setPasswordIndicatorEnabled(bool enabled, bool hideForEmptyInput, bool showImmediately)
{
    Q_D(QnInputField);

    bool update = d->hidePasswordIndicatorWhenEmpty != hideForEmptyInput;
    d->hidePasswordIndicatorWhenEmpty = hideForEmptyInput;

    if (enabled)
    {
        if (!d->passwordIndicator)
        {
            d->passwordIndicator = new QnPasswordStrengthIndicator(d->input);
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

bool QnInputField::isReadOnly() const
{
    Q_D(const QnInputField);
    return d->input->isReadOnly();
}

void QnInputField::setReadOnly(bool value)
{
    Q_D(QnInputField);
    d->input->setReadOnly(value);
}

bool QnInputField::validate()
{
    Q_D(QnInputField);
    d->validate();
    return lastValidationResult();
}

void QnInputField::updateDisplayState()
{
    Q_D(QnInputField);
    d->updateDisplayState();
}

void QnInputField::clear()
{
    Q_D(QnInputField);
    d->input->clear();
    d->validate();
}

bool QnInputField::isValid() const
{
    Q_D(const QnInputField);
    return d->validationResult().state == QValidator::Acceptable;
}

QValidator::State QnInputField::lastValidationResult() const
{
    Q_D(const QnInputField);
    return d->lastValidationResult.state;
}

void QnInputField::setValidator(Qn::TextValidateFunction validator, bool validateImmediately)
{
    Q_D(QnInputField);
    d->validator = validator;

    if (validateImmediately)
        d->validate();
}

void QnInputField::reset()
{
    Q_D(QnInputField);
    d->clearValidationResult();
    d->setHintText(QString());
    if (d->passwordIndicator)
        d->passwordIndicator->setVisible(false);
}

AbstractAccessor* QnInputField::createLabelWidthAccessor()
{
    return new LabelWidthAccessor();
}

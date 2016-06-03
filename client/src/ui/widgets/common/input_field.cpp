#include "input_field.h"

#include <ui/common/accessor.h>
#include <ui/style/custom_style.h>

class QnInputFieldPrivate : public QObject
{
public:
    QnInputFieldPrivate(QWidget* parent) :
        QObject(parent),
        parent(parent),
        title(new QLabel(parent)),
        hint(new QLabel(parent)),
        input(new QLineEdit(parent)),
        validator()
    {
        input->installEventFilter(this);
    }

    virtual bool eventFilter(QObject* watched, QEvent* event)
    {
        /* On focus make input look usual even if there is error. Hint will be visible though. */
        if (event->type() == QEvent::FocusIn && watched == input)
        {
            validate();
            input->setPalette(parent->palette());
        }

        /* Always pass event further */
        return false;
    }

    void validate()
    {
        if (!validator)
            return;

        auto result = validator(input->text());
        QString hintText = result.errorMessage;

        hint->setText(hintText);

        bool hideHint = hintText.isEmpty();
        if (hideHint != hint->isHidden())
        {
            hint->setVisible(!hideHint);
            parent->layout()->activate();
        }

        QPalette palette = parent->palette();
        if (result.state == QValidator::Invalid)
            setWarningStyle(&palette);

        input->setPalette(palette);
        hint->setPalette(palette);
    }

    QWidget* parent;
    QLabel* title;
    QLabel* hint;
    QLineEdit* input;
    Qn::TextValidateFunction validator;
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

    QGridLayout* grid = new QGridLayout(this);
    grid->addWidget(d->title, 0, 0, 1, 1, Qt::AlignRight);
    grid->addWidget(d->input, 0, 1, 1, 1);
    grid->addWidget(d->hint,  1, 1, 1, 1);
    grid->setMargin(0);

    d->title->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    d->title->setVisible(false);
    d->hint->setVisible(false);

    connect(d->input, &QLineEdit::textChanged, this, &QnInputField::textChanged);
    connect(d->input, &QLineEdit::editingFinished, d, &QnInputFieldPrivate::validate);
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
    d->hint->setText(value);
    d->hint->setVisible(!value.isEmpty());
}

QString QnInputField::text() const
{
    Q_D(const QnInputField);
    return d->input->text();
}

void QnInputField::setText(const QString& value)
{
    Q_D(QnInputField);
    d->input->setText(value);
    d->validate();
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

void QnInputField::validate()
{
    Q_D(QnInputField);
    d->validate();
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
    if (!d->validator)
        return true;

    return d->validator(d->input->text()).state == QValidator::Acceptable;
}

void QnInputField::setValidator(Qn::TextValidateFunction validator)
{
    Q_D(QnInputField);
    d->validator = validator;
    d->validate();
}

AbstractAccessor* QnInputField::createLabelWidthAccessor()
{
    return new LabelWidthAccessor();
}

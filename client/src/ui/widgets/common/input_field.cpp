#include "input_field.h"

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

    }

    void validate()
    {
        if (!validator)
            return;

        auto result = validator(input->text());
        bool isValid = std::get<0>(result);
        QString hintText = std::get<1>(result);

        hint->setText(hintText);
        hint->setVisible(!hintText.isEmpty());
        QPalette palette = parent->palette();
        if (!isValid)
            setWarningStyle(&palette);
        input->setPalette(palette);
        hint->setPalette(palette);
    }

    QWidget* parent;
    QLabel* title;
    QLabel* hint;
    QLineEdit* input;
    QnInputField::ValidateFunction validator;
    QMetaObject::Connection validatorConnection;

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

    d->title->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    d->title->setVisible(false);
    d->hint->setVisible(false);

    connect(d->input, &QLineEdit::textChanged, this, &QnInputField::textChanged);
}

QnInputField::~QnInputField()
{

}

QLabel* QnInputField::titleLabel() const
{
    Q_D(const QnInputField);
    return d->title;
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

    return std::get<0>(d->validator(d->input->text()));
}

void QnInputField::setValidator(ValidateFunction validator)
{
    Q_D(QnInputField);
    disconnect(d->validatorConnection);
    d->validator = validator;
    d->validatorConnection = connect(d->input, &QLineEdit::editingFinished, d, &QnInputFieldPrivate::validate);
    d->validate();
}

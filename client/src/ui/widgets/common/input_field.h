#pragma once

#include <QtWidgets/QWidget>

#include <ui/utils/validators.h>

class QnInputFieldPrivate;
class AbstractAccessor;

/**
 * Common class for various input fields.
 * Can check input for validity and display warning hint if something wrong.
 */
class QnInputField : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QLineEdit::EchoMode echoMode READ echoMode WRITE setEchoMode)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef QWidget base_type;

public:
    QnInputField(QWidget* parent = nullptr);
    virtual ~QnInputField();

    QString title() const;
    void setTitle(const QString& value);

    QString hint() const;
    void setHint(const QString& value);

    QString text() const;
    void setText(const QString& value);
    void clear();

    QLineEdit::EchoMode echoMode() const;
    void setEchoMode(QLineEdit::EchoMode value);

    bool isReadOnly() const;
    void setReadOnly(bool value);

    void validate();

    bool isValid() const;
    void setValidator(Qn::TextValidateFunction validator);

    static AbstractAccessor* createLabelWidthAccessor();

signals:
    void textChanged(const QString& text);

private:
    friend class LabelWidthAccessor;
    QScopedPointer<QnInputFieldPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnInputField)
    Q_DISABLE_COPY(QnInputField)
};

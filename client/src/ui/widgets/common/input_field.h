#pragma once

#include <QtWidgets/QWidget>

#include <ui/utils/validators.h>

class QnPasswordStrengthIndicator;
class QnInputFieldPrivate;
class AbstractAccessor;

/**
 * Common class for various input fields.
 * Can check input for validity and display warning hint if something wrong.
 */
class QnInputField : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
    Q_PROPERTY(QString hint READ hint WRITE setHint)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(QLineEdit::EchoMode echoMode READ echoMode WRITE setEchoMode)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(bool passwordIndicatorEnabled READ passwordIndicatorEnabled WRITE setPasswordIndicatorEnabled)

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

    QString placeholderText() const;
    void setPlaceholderText(const QString& value);

    QLineEdit::EchoMode echoMode() const;
    void setEchoMode(QLineEdit::EchoMode value);

    const QnPasswordStrengthIndicator* passwordIndicator() const;
    bool passwordIndicatorEnabled() const;
    void setPasswordIndicatorEnabled(bool enabled, bool hideForEmptyInput = true, bool showImmediately = false);

    bool isReadOnly() const;
    void setReadOnly(bool value);

    bool validate();

    bool isValid() const;
    bool lastValidationResult() const;

    void setValidator(Qn::TextValidateFunction validator, bool validateImmediately = false);

    bool emptyInputAllowed() const;
    const QString& emptyInputHint() const;
    void setEmptyInputAllowed(bool enabled, const QString& hint = QString());

    bool terminalSpacesAllowed() const;
    const QString& terminalSpacesHint() const;
    void setTerminalSpacesAllowed(bool allow, const QString& hint = QString());

    void setPasswordMode(QLineEdit::EchoMode echoMode, bool allowEmptyPassword, bool showStrengthIndicator);

    bool confirmationMode() const;
    const QnInputField* confirmationPrimaryField() const;
    const QString& confirmationFailureHint() const;
    void setConfirmationMode(const QnInputField* primaryField, const QString& hint = QString());

    void reset();

    static AbstractAccessor* createLabelWidthAccessor();

signals:
    void textChanged(const QString& text);

private:
    friend class LabelWidthAccessor;
    QScopedPointer<QnInputFieldPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnInputField)
    Q_DISABLE_COPY(QnInputField)
};

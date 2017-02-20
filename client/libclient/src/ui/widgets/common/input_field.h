#pragma once

#include <QtWidgets/QWidget>

#include <ui/utils/validators.h>
#include <utils/common/connective.h>

class QnPasswordStrengthIndicator;
class QnInputFieldPrivate;
class AbstractAccessor;

/**
 * Common class for various input fields.
 * Can check input for validity and display warning hint if something wrong.
 */
class QnInputField : public Connective<QWidget>
{
    Q_OBJECT

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
    Q_PROPERTY(QString hint READ hint WRITE setHint)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(QLineEdit::EchoMode echoMode READ echoMode WRITE setEchoMode)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(bool passwordIndicatorEnabled READ passwordIndicatorEnabled WRITE setPasswordIndicatorEnabled)

    typedef Connective<QWidget> base_type;

public:
    explicit QnInputField(QWidget* parent = nullptr);
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

    QString inputMask() const;
    void setInputMask(const QString& inputMask);

    const QnPasswordStrengthIndicator* passwordIndicator() const;
    bool passwordIndicatorEnabled() const;
    void setPasswordIndicatorEnabled(bool enabled, bool hideForEmptyInput = true, bool showImmediately = false);

    bool isReadOnly() const;
    void setReadOnly(bool value);

    bool validate();

    /* After a small delay calls validate if input is not being edited. */
    void updateDisplayStateDelayed();

    bool isValid() const;
    QValidator::State lastValidationResult() const;

    void setValidator(Qn::TextValidateFunction validator, bool validateImmediately = false);
    void reset();

    static AbstractAccessor* createLabelWidthAccessor();

signals:
    void textChanged(const QString& text);
    void editingFinished();

private:
    friend class LabelWidthAccessor;
    QScopedPointer<QnInputFieldPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnInputField)
    Q_DISABLE_COPY(QnInputField)
};

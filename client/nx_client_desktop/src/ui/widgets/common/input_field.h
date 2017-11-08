#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>

#include <ui/utils/validators.h>
#include <ui/utils/password_information.h>
#include <nx/utils/password_analyzer.h>
#include <utils/common/connective.h>

#include <nx/client/desktop/ui/common/detail/base_input_field.h>

class QnPasswordStrengthIndicator;
class QnInputFieldPrivate;
class AbstractAccessor;

/**
 * Common class for various input fields.
 * Can check input for validity and display warning hint if something wrong.
 */
class QnInputField : public nx::client::desktop::ui::detail::BaseInputField
{
    Q_OBJECT

    Q_PROPERTY(QLineEdit::EchoMode echoMode READ echoMode WRITE setEchoMode)
    Q_PROPERTY(bool passwordIndicatorEnabled READ passwordIndicatorEnabled WRITE setPasswordIndicatorEnabled)

    using base_type = nx::client::desktop::ui::detail::BaseInputField;

public:
    static QnInputField* create(
        const QString& text,
        const Qn::TextValidateFunction& validator,
        QWidget* parent = nullptr);

    explicit QnInputField(QWidget* parent = nullptr);
    virtual ~QnInputField();

    QLineEdit::EchoMode echoMode() const;
    void setEchoMode(QLineEdit::EchoMode value);

    QString inputMask() const;
    void setInputMask(const QString& inputMask);

    const QnPasswordStrengthIndicator* passwordIndicator() const;
    bool passwordIndicatorEnabled() const;
    void setPasswordIndicatorEnabled(
        bool enabled,
        bool hideForEmptyInput = true,
        bool showImmediately = false,
        QnPasswordInformation::AnalyzeFunction analyzeFunction = nx::utils::passwordStrength);

signals:
    void editingFinished();

protected:
    virtual Qn::ValidationResult calculateValidationResult() const override;

private:
    friend class LabelWidthAccessor;
    QScopedPointer<QnInputFieldPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnInputField)
    Q_DISABLE_COPY(QnInputField)
};

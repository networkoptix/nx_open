#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/client/desktop/common/utils/password_information.h>
#include <nx/utils/password_analyzer.h>
#include <utils/common/connective.h>

#include <nx/vms/client/desktop/common/widgets/detail/base_input_field.h>

class AbstractAccessor;

namespace nx::vms::client::desktop {

class PasswordStrengthIndicator;

/**
 * Common class for various input fields.
 * Can check input for validity and display warning hint if something wrong.
 */
class InputField: public detail::BaseInputField
{
    Q_OBJECT

    Q_PROPERTY(QLineEdit::EchoMode echoMode READ echoMode WRITE setEchoMode)
    Q_PROPERTY(bool passwordIndicatorEnabled READ passwordIndicatorEnabled WRITE setPasswordIndicatorEnabled)

    using base_type = detail::BaseInputField;

public:
    static InputField* create(
        const QString& text,
        const TextValidateFunction& validator,
        QWidget* parent = nullptr);

    explicit InputField(QWidget* parent = nullptr);
    virtual ~InputField();

    QLineEdit::EchoMode echoMode() const;
    void setEchoMode(QLineEdit::EchoMode value);

    QString inputMask() const;
    void setInputMask(const QString& inputMask);

    const PasswordStrengthIndicator* passwordIndicator() const;
    bool passwordIndicatorEnabled() const;
    void setPasswordIndicatorEnabled(
        bool enabled,
        bool hideForEmptyInput = true,
        bool showImmediately = false,
        PasswordInformation::AnalyzeFunction analyzeFunction = nx::utils::passwordStrength);

    void useForPassword();

    QLineEdit* lineEdit();
    const QLineEdit* lineEdit() const;

signals:
    void editingFinished();

protected:
    virtual ValidationResult calculateValidationResult() const override;

private:
    friend class LabelWidthAccessor;
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop

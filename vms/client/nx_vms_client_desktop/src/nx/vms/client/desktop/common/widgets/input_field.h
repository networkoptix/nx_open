// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/common/utils/validators.h>

#include <nx/vms/client/desktop/common/widgets/detail/base_input_field.h>

class AbstractAccessor;

namespace nx::vms::client::desktop {

/**
 * Common class for various input fields.
 * Can check input for validity and display warning hint if something wrong.
 * For password input use `PasswordInputField` class.
 */
class InputField: public detail::BaseInputField
{
    Q_OBJECT
    using base_type = detail::BaseInputField;

public:
    static InputField* create(
        const QString& text,
        const TextValidateFunction& validator,
        ValidationBehavior validationBehavior = ValidationBehavior::validateOnInputShowOnFocus,
        QWidget* parent = nullptr);

    explicit InputField(
        ValidationBehavior validationBehavior = ValidationBehavior::validateOnInputShowOnFocus,
        QWidget* parent = nullptr);
    explicit InputField(QWidget* parent = nullptr);
    virtual ~InputField();

    QString inputMask() const;
    void setInputMask(const QString& inputMask);

    QLineEdit* lineEdit();
    const QLineEdit* lineEdit() const;

signals:
    void editingFinished();
};

} // namespace nx::vms::client::desktop

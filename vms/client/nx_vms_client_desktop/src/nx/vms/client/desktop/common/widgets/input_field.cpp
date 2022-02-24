// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "input_field.h"

#include <QtGui/QFocusEvent>
#include <QtWidgets/QBoxLayout>

#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <ui/widgets/word_wrapped_label.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

detail::BaseInputField::AccessorPtr accessor(const QByteArray& propertyName)
{
    return detail::BaseInputField::AccessorPtr(new PropertyAccessor(propertyName));
}

} // namespace

InputField* InputField::create(
    const QString& text,
    const TextValidateFunction& validator,
    ValidationBehavior validationBehavior,
    QWidget* parent)
{
    const auto result = new InputField(validationBehavior, parent);
    result->setValidator(validator);
    result->setText(text);
    return result;
}

InputField::InputField(
    ValidationBehavior validationBehavior,
    QWidget* parent):
    base_type(new QLineEdit(),
        accessor("text"),
        accessor("readOnly"),
        accessor("placeholderText"),
        /* useWarningStyleForControl */ true,
        validationBehavior,
        parent)
{
    const auto lineEdit = qobject_cast<QLineEdit*>(input());
    connect(lineEdit, &QLineEdit::editingFinished, this, &InputField::editingFinished);
    connect(lineEdit, &QLineEdit::textChanged, this, &InputField::textChanged);
}

InputField::InputField(QWidget* parent):
    base_type(new QLineEdit(),
        accessor("text"),
        accessor("readOnly"),
        accessor("placeholderText"),
        /* useWarningStyleForControl */ true,
        ValidationBehavior::validateOnInputShowOnFocus,
        parent)
{
    const auto lineEdit = qobject_cast<QLineEdit*>(input());
    connect(lineEdit, &QLineEdit::editingFinished, this, &InputField::editingFinished);
    connect(lineEdit, &QLineEdit::textChanged, this, &InputField::textChanged);
}

InputField::~InputField()
{
}

QString InputField::inputMask() const
{
    return lineEdit()->inputMask();
}

void InputField::setInputMask(const QString& inputMask)
{
    lineEdit()->setInputMask(inputMask);
}

QLineEdit* InputField::lineEdit()
{
    return dynamic_cast<QLineEdit*>(input());
}

const QLineEdit* InputField::lineEdit() const
{
    return dynamic_cast<const QLineEdit*>(input());
}

} // namespace nx::vms::client::desktop

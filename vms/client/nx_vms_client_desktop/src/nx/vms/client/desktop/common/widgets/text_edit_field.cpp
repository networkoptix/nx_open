// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_edit_field.h"

#include <QtWidgets/QTextEdit>

#include <nx/vms/client/desktop/common/utils/accessor.h>

namespace nx::vms::client::desktop {

TextEditField* TextEditField::create(
    const QString& text,
    const TextValidateFunction& validator,
    QWidget* parent)
{
    const auto result = new TextEditField(parent);
    result->setValidator(validator);
    result->setText(text);
    return result;
}

TextEditField::TextEditField(QWidget* parent):
    base_type(new QTextEdit(),
        PropertyAccessor::create("plainText"),
        PropertyAccessor::create("readOnly"),
        PropertyAccessor::create("placeholderText"),
        /* useWarningStyleForControl */ true,
        ValidationBehavior::validateOnInputShowOnFocus,
        parent)
{
    const auto textInput = qobject_cast<QTextEdit*>(input());
    connect(textInput, &QTextEdit::textChanged, this, [this]()
    {
        emit textChanged(text());
    });
}

} // namespace nx::vms::client::desktop

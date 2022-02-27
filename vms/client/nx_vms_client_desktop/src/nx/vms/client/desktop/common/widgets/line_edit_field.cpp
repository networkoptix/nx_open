// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "line_edit_field.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QFocusEvent>

#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <nx/vms/client/desktop/common/widgets/line_edit_controls.h>
#include <nx/vms/client/desktop/common/widgets/clipboard_button.h>

namespace nx::vms::client::desktop {

LineEditField* LineEditField::create(
    const QString& text,
    const TextValidateFunction& validator,
    QWidget* parent)
{
    const auto result = new LineEditField(parent);
    result->setValidator(validator);
    result->setText(text);
    return result;
}

LineEditField::LineEditField(QWidget* parent):
    base_type(new QLineEdit(),
        PropertyAccessor::create("text"),
        PropertyAccessor::create("readOnly"),
        PropertyAccessor::create("placeholderText"),
        /* useWarningStyleForControl */ false,
        ValidationBehavior::validateOnInput,
        parent)
{
    const auto textInput = qobject_cast<QLineEdit*>(input());
    connect(textInput, &QLineEdit::textChanged, this, [this]()
    {
        emit textChanged(text());
    });

    QPushButton* copyButton = new ClipboardButton(this);
    LineEditControls::get(textInput)->addControl(copyButton);

    QObject::connect(copyButton, &QPushButton::pressed, [this]
    {
        QApplication::clipboard()->setText(text());
    });
}

} // namespace nx::vms::client::desktop

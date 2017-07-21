#include "text_edit_field.h"

#include <QtWidgets/QTextEdit>

#include <ui/common/accessor.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

TextEditField* TextEditField::create(
    const QString& text,
    const Qn::TextValidateFunction& validator,
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
        true,
        parent)
{
    const auto textInput = qobject_cast<QTextEdit*>(input());
    connect(textInput, &QTextEdit::textChanged, this, [this]()
    {
        emit textChanged(text());
    });
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx


#include "text_edit_field.h"

#include <QtWidgets/QTextEdit>

#include <ui/common/accessor.h>

namespace {

using namespace nx::client::desktop::ui::detail;

BaseInputField::AccessorPtr accessor(const QByteArray& propertyName)
{
    return BaseInputField::AccessorPtr(new PropertyAccessor(propertyName));
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

TextEditField::TextEditField(QWidget* parent):
    base_type(new QTextEdit(), accessor("plainText"),
        accessor("readOnly"), accessor("placeholderText"), parent)
{
    const auto textInput = qobject_cast<QTextEdit*>(input());
    connect(textInput, &QTextEdit::textChanged, this, [this]()
    {
        emit textChanged(text());
        validate();
    });
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx


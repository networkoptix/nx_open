#include "tool_button.h"
#include <ui/style/skin.h>

QnToolButton::QnToolButton(QWidget* parent): base_type(parent)
{
}

void QnToolButton::adjustIconSize()
{
    QIcon icon = this->icon();
    if (icon.isNull())
        return;

    setIconSize(QnSkin::maximumSize(icon));
}

void QnToolButton::mousePressEvent(QMouseEvent* event)
{
    base_type::mousePressEvent(event);
    emit justPressed();
}

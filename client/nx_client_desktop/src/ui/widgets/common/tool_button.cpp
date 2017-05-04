#include "tool_button.h"
#include <ui/style/skin.h>
#include <utils/common/delayed.h>


QnToolButton::QnToolButton(QWidget* parent): base_type(parent)
{
}

void QnToolButton::adjustIconSize()
{
    if (!icon().isNull())
       setIconSize(calculateIconSize());
}

QSize QnToolButton::calculateIconSize() const
{
    return QnSkin::maximumSize(icon());
}

void QnToolButton::mousePressEvent(QMouseEvent* event)
{
    base_type::mousePressEvent(event);

    /* Should finish mouse press processing before doing anything drastic
    * in the signal handler, otherwise bad things with mouse grab etc. may happen
    * especially when the button resides in graphics proxy widget: */
    auto emitJustPressed = [this] { emit justPressed(); };
    executeDelayedParented(emitJustPressed, 0, this);
}

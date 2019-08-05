#include "tool_button.h"

#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOptionToolButton>

#include <ui/style/skin.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

ToolButton::ToolButton(QWidget* parent): base_type(parent)
{
}

void ToolButton::adjustIconSize()
{
    if (!icon().isNull())
       setIconSize(calculateIconSize());
}

QSize ToolButton::calculateIconSize() const
{
    return QnSkin::maximumSize(icon());
}

void ToolButton::mousePressEvent(QMouseEvent* event)
{
    base_type::mousePressEvent(event);

    /* Should finish mouse press processing before doing anything drastic
    * in the signal handler, otherwise bad things with mouse grab etc. may happen
    * especially when the button resides in graphics proxy widget: */
    auto emitJustPressed = [this] { emit justPressed(); };
    executeLater(emitJustPressed, this);
}

void ToolButton::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter p(this);
    QStyleOptionToolButton option;
    initStyleOption(&option);

    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const bool pressed = option.state.testFlag(QStyle::State_Sunken) && enabled;
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver) && enabled && !pressed;
    const bool normal = enabled && !hovered && !pressed;

    if ((pressed && !m_drawnBackgrounds.testFlag(PressedBackground))
        || (hovered && !m_drawnBackgrounds.testFlag(HoveredBackground))
        || (normal && !m_drawnBackgrounds.testFlag(NormalBackground)))
    {
        option.palette.setColor(QPalette::Window, Qt::transparent);
    }

    p.drawComplexControl(QStyle::CC_ToolButton, option);
}

ToolButton::Backgrounds ToolButton::drawnBackgrounds() const
{
    return m_drawnBackgrounds;
}

void ToolButton::setDrawnBackgrounds(Backgrounds value)
{
    if (m_drawnBackgrounds == value)
        return;

    m_drawnBackgrounds = value;
    update();
}

} // namespace nx::vms::client::desktop

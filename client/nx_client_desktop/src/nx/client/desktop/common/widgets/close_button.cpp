#include "close_button.h"

namespace nx {
namespace client {
namespace desktop {

CloseButton::CloseButton(QWidget* parent) :
    HoverButton(lit("text_buttons/selectable_button_close.png"),
        lit("text_buttons/selectable_button_close_hovered.png"),
        lit("text_buttons/selectable_button_close_pressed.png"),
        parent)
{
    setFixedSize(HoverButton::sizeHint());
}

} // namespace desktop
} // namespace client
} // namespace nx

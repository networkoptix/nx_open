#include "panel.h"

namespace nx::vms::client::desktop {

Panel::Panel(QWidget* parent): QWidget(parent)
{
    setAutoFillBackground(true);
}

} // namespace nx::vms::client::desktop

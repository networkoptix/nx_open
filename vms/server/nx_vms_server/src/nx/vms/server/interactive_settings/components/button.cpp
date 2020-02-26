#include "button.h"

namespace nx::vms::server::interactive_settings::components {

Button::Button(QObject* parent):
    Item(QStringLiteral("Button"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components

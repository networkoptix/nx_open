#include "combo_box.h"

namespace nx::vms::server::interactive_settings::components {

ComboBox::ComboBox(QObject* parent):
    EnumerationItem(QStringLiteral("ComboBox"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components

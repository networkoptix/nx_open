#include "text_area.h"

namespace nx::vms::server::interactive_settings::components {

TextArea::TextArea(QObject* parent):
    ValueItem(QStringLiteral("TextArea"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components

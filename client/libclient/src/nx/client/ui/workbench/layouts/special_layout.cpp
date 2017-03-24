#include "special_layout.h"

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace layouts {

SpecialLayout::SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent):
    base_type(resource, parent)
{
    setFlags(flags() | QnLayoutFlag::SpecialBackground);
}

} // namespace layouts
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx


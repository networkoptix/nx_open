#include "special_layout.h"

#include <nx/client/ui/workbench/panels/special_layout_panel.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace layouts {

SpecialLayout::SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent):
    base_type(resource, parent)
{
    setFlags(flags() | QnLayoutFlag::SpecialBackground);
    //setPanel(panels::SpecailLayoutPanel(this));
}

} // namespace layouts
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx


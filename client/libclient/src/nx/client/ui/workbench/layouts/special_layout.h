#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_layout.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace layouts {

class SpecialLayout: public QnWorkbenchLayout
{
    using base_type = QnWorkbenchLayout;

public:
    SpecialLayout(const QnLayoutResourcePtr& resource, QObject* parent = nullptr);

private:
};

} // namespace layouts
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx

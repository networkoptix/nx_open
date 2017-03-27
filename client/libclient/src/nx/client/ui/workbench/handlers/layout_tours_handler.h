#pragma once

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutToursHandler: public QnWorkbenchContextAware, public QObject
{
    using base_type = QnWorkbenchContextAware;
public:
    LayoutToursHandler(QObject* parent = nullptr);

private:
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

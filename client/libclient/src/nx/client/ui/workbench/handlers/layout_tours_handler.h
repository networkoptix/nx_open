#pragma once

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace handlers {

class LayoutToursHandler: public QnWorkbenchContextAware, public QObject
{
    using base_type = QnWorkbenchContextAware;
public:
    LayoutToursHandler(QObject* parent = nullptr);

private:
};

} // namespace handlers
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx


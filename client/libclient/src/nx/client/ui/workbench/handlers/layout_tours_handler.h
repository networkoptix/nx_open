#pragma once

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace handlers {

class LayoutToursHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutToursHandler(QObject* parent = nullptr);

private:
    void openTousLayout();
};

} // namespace handlers
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx


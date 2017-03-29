#pragma once

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutToursHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutToursHandler(QObject* parent = nullptr);

private:
    void openTousLayout();
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

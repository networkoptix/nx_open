#pragma once

#include <QtCore/QObject>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchLayout;

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

    void setupToursLayout(QnWorkbenchLayout* layout);
};

} // namespace handlers
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx


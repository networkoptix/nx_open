#pragma once

class QnResourceWidget;
class QnWorkbenchContext;
class QnWorkbenchItem;

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class ResourceWidgetFactory
{
public:
    static QnResourceWidget* createWidget(QnWorkbenchContext* context, QnWorkbenchItem* item);
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop

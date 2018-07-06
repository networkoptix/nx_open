#include "tab_widget.h"

namespace nx {
namespace client {
namespace desktop {

TabWidget::TabWidget(QWidget* parent):
    TabWidget(nullptr, parent)
{
}

TabWidget::TabWidget(QTabBar* tabBar, QWidget* parent):
    base_type(parent)
{
    if (tabBar)
        setTabBar(tabBar);
}

} // namespace desktop
} // namespace client
} // namespace nx

#include "widget_utils.h"

#include <QtWidgets/QLayout>

namespace nx {
namespace client {
namespace desktop {

void WidgetUtils::clearLayout(QLayout* layout)
{
    if (!layout)
        return;

    while (const auto item = layout->takeAt(0))
    {
        if (const auto widget = item->widget())
            delete widget;
        else if (const auto childLayout = item->layout())
            removeLayout(childLayout);
        delete item;
    }
}

} // namespace desktop
} // namespace client
} // namespace nx

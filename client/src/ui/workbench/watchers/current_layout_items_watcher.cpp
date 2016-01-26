
#include "current_layout_items_watcher.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>

QnCurrentLayoutItemsWatcher::QnCurrentLayoutItemsWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

{
    const auto onLayoutChangedHandler = [this]()
    {
        emit layoutChanged();

        const auto layout = workbench()->currentLayout();
        if (!layout)
            return;

        const auto items = layout->items();
        for (const auto item: items)
            emit itemAdded(item);

        connect(layout, &QnWorkbenchLayout::itemAdded, this, &QnCurrentLayoutItemsWatcher::itemAdded);
        connect(layout, &QnWorkbenchLayout::itemRemoved, this, &QnCurrentLayoutItemsWatcher::itemRemoved);
    };

    const auto onLayoutAboutToBeChangedHandler = [this]()
    {
        emit layoutAboutToBeChanged();

        const auto layout = workbench()->currentLayout();
        if (!layout)
            return;

        disconnect(layout, nullptr, this, nullptr);

        const auto items = layout->items();
        for (const auto item: items)
            emit itemAboutToBeRemoved(item);
    };

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this, onLayoutChangedHandler);
    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this, onLayoutAboutToBeChangedHandler);
}

QnCurrentLayoutItemsWatcher::~QnCurrentLayoutItemsWatcher()
{
}

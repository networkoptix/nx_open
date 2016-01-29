
#include "workbench_items_watcher.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>

QnWorkbenchItemsWatcher::QnWorkbenchItemsWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

{
    const auto addLayoutHandler = [this](QnWorkbenchLayout *layout)
    {
        const auto items = layout->items();
        for (const auto &item: items)
            emit itemAdded(item);

        connect(layout, &QnWorkbenchLayout::itemAdded, this, [this](QnWorkbenchItem *item)
        {
            emit itemAdded(item);

            if (item->layout() == workbench()->currentLayout())
                emit itemShown(item);
        });

        connect(layout, &QnWorkbenchLayout::itemRemoved, this, [this](QnWorkbenchItem *item)
        {
            if (item->layout() == workbench()->currentLayout())
                emit itemHidden(item);

            emit itemRemoved(item);
        });
    };

    const auto removeLayoutHandler = [this](QnWorkbenchLayout *layout)
    {
        disconnect(layout, nullptr, this, nullptr);
        disconnect(layout, nullptr, this, nullptr);

        const auto items = layout->items();
        for (const auto &item: items)
            emit itemRemoved(item);
    };

    const auto showCurrentItemsHandler = [this]()
    {
        emit layoutChanged();

        const auto layout = workbench()->currentLayout();
        if (!layout)
            return;

        const auto items = layout->items();
        for (const auto &item: items)
            emit itemShown(item);
    };

    const auto hideCurrentItemsHandler = [this]()
    {
        emit layoutAboutToBeChanged();

        const auto layout = workbench()->currentLayout();
        if (!layout)
            return;

        const auto items = layout->items();
        for (const auto item: items)
            emit itemHidden(item);
    };

    connect(workbench(), &QnWorkbench::layoutAdded, this, addLayoutHandler);
    connect(workbench(), &QnWorkbench::layoutRemoved, this, removeLayoutHandler);

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this, showCurrentItemsHandler);
    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this, hideCurrentItemsHandler);
}

QnWorkbenchItemsWatcher::~QnWorkbenchItemsWatcher()
{
}

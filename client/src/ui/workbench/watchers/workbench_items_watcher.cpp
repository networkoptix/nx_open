
#include "workbench_items_watcher.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

class QnWorkbenchItem;

QnWorkbenchLayoutItemsWatcher::QnWorkbenchLayoutItemsWatcher(QObject *parent) :
    QObject(parent),
    base_type(parent),
    m_connections(),
    m_itemsCount(0)
{
    connect(workbench(), &QnWorkbench::layoutAdded, 
        this, &QnWorkbenchLayoutItemsWatcher::onLayoutAdded);
    connect(workbench(), &QnWorkbench::layoutRemoved,
        this, &QnWorkbenchLayoutItemsWatcher::onLayoutRemoved);
}

QnWorkbenchLayoutItemsWatcher::~QnWorkbenchLayoutItemsWatcher()
{}

int QnWorkbenchLayoutItemsWatcher::itemsCount() const
{
    return m_itemsCount;
}

void QnWorkbenchLayoutItemsWatcher::setItemsCount(int count)
{
    if (m_itemsCount == count)
        return;

    NX_ASSERT(count >= 0, "Invalid items count");

    m_itemsCount = count;
    emit itemsCountChanged();
}

void QnWorkbenchLayoutItemsWatcher::incItemsCount(int diff)
{
    setItemsCount(itemsCount() + diff);
}

void QnWorkbenchLayoutItemsWatcher::decItemsCount(int diff)
{
    setItemsCount(itemsCount() - diff);
}

void QnWorkbenchLayoutItemsWatcher::onLayoutAdded(QnWorkbenchLayout *layout)
{
    const bool exist = (m_connections.contains(layout));
    NX_ASSERT(!exist, "Layout has been added already");
    if (exist)
        return;

    const auto connectionData = QnDisconnectHelperPtr(new QnDisconnectHelper());
    *connectionData << connect(layout, &QnWorkbenchLayout::itemAdded, this,
        [this]() { incItemsCount(); });
    *connectionData << connect(layout, &QnWorkbenchLayout::itemRemoved, this,
        [this]() { decItemsCount(); });

    m_connections.insert(layout, connectionData);
    incItemsCount(layout->items().count());
}

void QnWorkbenchLayoutItemsWatcher::onLayoutRemoved(QnWorkbenchLayout *layout)
{
    if (m_connections.remove(layout))
        decItemsCount(layout->items().size());
}


#include "tabbar.h"

#include <QtGui/QTabWidget>

TabBar::TabBar(QWidget *parent) : QTabBar(parent)
{
}

void TabBar::tabInserted(int index)
{
    Q_EMIT tabInserted(index, index);
    Q_EMIT countChanged(count());
}

void TabBar::tabRemoved(int index)
{
    Q_EMIT tabRemoved(index, index);
    Q_EMIT countChanged(count());
}

void TabBar::_q_removeTab(int index)
{
    if (QTabWidget *tabWidget = qobject_cast<QTabWidget *>(parentWidget()))
        tabWidget->removeTab(index);
    else
        removeTab(index);
}

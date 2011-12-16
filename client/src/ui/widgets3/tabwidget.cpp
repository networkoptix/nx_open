#include "tabwidget.h"

#include <QtGui/QTabBar>

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent)
{
}

TabWidget::SelectionBehavior TabWidget::selectionBehaviorOnRemove() const
{
    return static_cast<TabWidget::SelectionBehavior>(tabBar()->selectionBehaviorOnRemove());
}

void TabWidget::setSelectionBehaviorOnRemove(TabWidget::SelectionBehavior behavior)
{
    tabBar()->setSelectionBehaviorOnRemove(static_cast<QTabBar::SelectionBehavior>(behavior));
}

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QtGui/QTabWidget>

class QTabBar;

class TabWidget : public QTabWidget
{
    Q_OBJECT
    Q_PROPERTY(SelectionBehavior selectionBehaviorOnRemove READ selectionBehaviorOnRemove WRITE setSelectionBehaviorOnRemove)

public:
    explicit TabWidget(QWidget *parent = 0);

    enum SelectionBehavior {
        SelectLeftTab,
        SelectRightTab,
        SelectPreviousTab
    };

    SelectionBehavior selectionBehaviorOnRemove() const;
    void setSelectionBehaviorOnRemove(SelectionBehavior behavior);
};

#endif // TABWIDGET_H

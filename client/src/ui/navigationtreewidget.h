#ifndef NAVIGATIONTREEWIDGET_H
#define NAVIGATIONTREEWIDGET_H

#include <QtGui/QWidget>
#include "ui/processors/dragprocesshandler.h"

class QAbstractItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTabWidget;
class QToolButton;
class QTreeView;

class NavigationTreeWidget : public QWidget, protected DragProcessHandler
{
    Q_OBJECT

public:
    NavigationTreeWidget(QWidget *parent = 0);
    ~NavigationTreeWidget();

    bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    void contextMenuEvent(QContextMenuEvent *event);

    virtual void startDrag(DragInfo *info) override;

Q_SIGNALS:
    void activated(uint resourceId);

private Q_SLOTS:
    void filterChanged(const QString &filter);
    void itemActivated(const QModelIndex &index);
    void open();

private:
    QTabWidget *m_tabWidget;
    QToolButton *m_previousItemButton;
    QToolButton *m_nextItemButton;

    QLineEdit *m_filterLineEdit;
    QToolButton *m_clearFilterButton;

    QAbstractItemModel *m_resourcesModel;
    QTreeView *m_resourcesTreeView;
    QSortFilterProxyModel *m_searchProxyModel;
    QTreeView *m_searchTreeView;

    DragProcessor *m_dragProcessor;
};

#endif // NAVIGATIONTREEWIDGET_H

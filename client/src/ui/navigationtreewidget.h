#ifndef NAVIGATIONTREEWIDGET_H
#define NAVIGATIONTREEWIDGET_H

#include <QtCore/QPointer>

#include <QtGui/QWidget>

class QLineEdit;
class QTabWidget;
class QToolButton;
class QTreeView;
class QModelIndex;

class QnWorkbenchController;
class QnWorkbenchItem;

class ResourceModel;
class ResourceSortFilterProxyModel;

class NavigationTreeWidget : public QWidget
{
    Q_OBJECT

public:
    NavigationTreeWidget(QWidget *parent = 0);
    ~NavigationTreeWidget();

    void setWorkbenchController(QnWorkbenchController *controller);

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void wheelEvent(QWheelEvent *event);
    void timerEvent(QTimerEvent *event);

Q_SIGNALS:
    void activated(uint resourceId);

    void newTabRequested();

private Q_SLOTS:
    void filterChanged(const QString &filter);
    void itemActivated(const QModelIndex &index);
    void open();

    void workbenchLayoutAboutToBeChanged();
    void workbenchLayoutChanged();
    void workbenchLayoutItemAdded(QnWorkbenchItem *item);
    void workbenchLayoutItemRemoved(QnWorkbenchItem *item);

    void handleInsertRows(const QModelIndex &parent, int first, int last);
    void handleRemoveRows(const QModelIndex &parent, int first, int last);

private:
    QTabWidget *m_tabWidget;
    QToolButton *m_previousItemButton;
    QToolButton *m_nextItemButton;

    QLineEdit *m_filterLineEdit;
    QToolButton *m_clearFilterButton;
    int m_filterTimerId;
    bool m_dontSyncWithLayout;

    ResourceModel *m_resourcesModel;
    QTreeView *m_resourcesTreeView;
    QPointer<ResourceSortFilterProxyModel> m_searchProxyModel;
    QTreeView *m_searchTreeView;

    QPointer<QnWorkbenchController> m_controller;

    friend class NavigationTreeItemDelegate;
};

#endif // NAVIGATIONTREEWIDGET_H

#ifndef NAVIGATIONTREEWIDGET_H
#define NAVIGATIONTREEWIDGET_H

#include <QtCore/QPointer>

#include <QtGui/QWidget>

class QAbstractItemModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTabWidget;
class QTabBar;
class QToolButton;
class QTreeView;
class QModelIndex;

class QnWorkbenchController;

class NavigationTreeWidget : public QWidget
{
    Q_OBJECT

public:
    NavigationTreeWidget(QWidget *parent = 0);
    ~NavigationTreeWidget();

    void setWorkbenchController(QnWorkbenchController *controller);

    QTabBar *tabBar() const;

protected:
    void contextMenuEvent(QContextMenuEvent *event);
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
    void handleInsertRows(const QModelIndex &parent, int first, int last);
    void handleRemoveRows(const QModelIndex &parent, int first, int last);

private:
    QTabWidget *m_tabWidget;
    QToolButton *m_previousItemButton;
    QToolButton *m_nextItemButton;

    QLineEdit *m_filterLineEdit;
    QToolButton *m_clearFilterButton;
    int m_filterTimerId;

    QAbstractItemModel *m_resourcesModel;
    QTreeView *m_resourcesTreeView;
    QPointer<QSortFilterProxyModel> m_searchProxyModel;
    QTreeView *m_searchTreeView;

    QPointer<QnWorkbenchController> m_controller;

    friend class NavigationTreeItemDelegate;
};

#endif // NAVIGATIONTREEWIDGET_H

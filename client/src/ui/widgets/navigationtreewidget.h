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

class QnResourceModel;
class QnResourceSearchProxyModel;

class NavigationTreeWidget : public QWidget
{
    Q_OBJECT

public:
    NavigationTreeWidget(QWidget *parent = 0);
    ~NavigationTreeWidget();

    void setWorkbenchController(QnWorkbenchController *controller);

signals:
    void activated(uint resourceId);
    void newTabRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void timerEvent(QTimerEvent *event);

private slots:
    void open();

    void at_filterLineEdit_textChanged(const QString &filter);
    void at_treeView_activated(const QModelIndex &index);

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

    QnResourceModel *m_resourcesModel;
    QTreeView *m_resourcesTreeView;
    QPointer<QnResourceSearchProxyModel> m_searchProxyModel;
    QTreeView *m_searchTreeView;

    QPointer<QnWorkbenchController> m_controller;

    friend class NavigationTreeItemDelegate;
};

#endif // NAVIGATIONTREEWIDGET_H

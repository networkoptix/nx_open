#ifndef QN_RESOURCE_TREE_WIDGET_H
#define QN_RESOURCE_TREE_WIDGET_H

#include <QWidget>
#include <core/resource/resource_fwd.h>

class QLineEdit;
class QTabWidget;
class QToolButton;
class QTreeView;
class QModelIndex;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;

class QnResourceCriterion;
class QnResourceModel;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizer;
class QnResourceTreeItemDelegate;

class QnResourceTreeWidget: public QWidget {
    Q_OBJECT

public:
    QnResourceTreeWidget(QWidget *parent = 0);
    ~QnResourceTreeWidget();

    void setWorkbench(QnWorkbench *workbench);

    QnResourceList selectedResources() const;

signals:
    void activated(const QnResourcePtr &resource);
    void newTabRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void timerEvent(QTimerEvent *event);

    QnResourceSearchProxyModel *layoutModel(QnWorkbenchLayout *layout, bool create) const;
    QnResourceSearchSynchronizer *layoutSynchronizer(QnWorkbenchLayout *layout, bool create) const;
    QString layoutSearchString(QnWorkbenchLayout *layout) const;
    void setLayoutSearchString(QnWorkbenchLayout *layout, const QString &searchString) const;
    QnResourceCriterion *layoutCriterion(QnWorkbenchLayout *layout) const;
    void setLayoutCriterion(QnWorkbenchLayout *layout, QnResourceCriterion *criterion) const;

    void killSearchTimer();

private slots:
    void open();

    void at_filterLineEdit_textChanged(const QString &filter);
    void at_treeView_activated(const QModelIndex &index);
    void at_tabWidget_currentChanged(int index);

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    void at_workbench_aboutToBeDestroyed();

private:
    QTabWidget *m_tabWidget;
    QToolButton *m_previousItemButton;
    QToolButton *m_nextItemButton;

    QLineEdit *m_filterLineEdit;
    bool m_ignoreFilterChanges;

    QToolButton *m_clearFilterButton;
    int m_filterTimerId;

    QnResourceModel *m_resourceModel;
    QTreeView *m_resourceTreeView;
    QTreeView *m_searchTreeView;

    QnResourceTreeItemDelegate *m_resourceDelegate;
    QnResourceTreeItemDelegate *m_searchDelegate;

    QnWorkbench *m_workbench;
};

#endif // QN_RESOURCE_TREE_WIDGET_H

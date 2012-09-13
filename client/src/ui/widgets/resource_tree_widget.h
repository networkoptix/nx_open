#ifndef QN_RESOURCE_TREE_WIDGET_H
#define QN_RESOURCE_TREE_WIDGET_H

#include <QWidget>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <ui/actions/action_target_provider.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_globals.h>

class QLineEdit;
class QTabWidget;
class QToolButton;
class QTreeView;
class QModelIndex;
class QItemSelectionModel;
class QSortFilterProxyModel;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnWorkbenchContext;

class QnResourceCriterion;
class QnResourcePoolModel;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizer;
class QnResourceTreeItemDelegate;

namespace Ui {
    class ResourceTreeWidget;
}

class QnResourceTreeWidget: public QWidget, public QnWorkbenchContextAware, public QnActionTargetProvider {
    Q_OBJECT;

public:
    enum Tab {
        ResourcesTab,
        SearchTab,
        TabCount
    };

    QnResourceTreeWidget(QWidget *parent = NULL, QnWorkbenchContext *context = NULL);

    virtual ~QnResourceTreeWidget();

    Tab currentTab() const;

    void setCurrentTab(Tab tab);

    QnResourceList selectedResources() const;

    QnLayoutItemIndexList selectedLayoutItems() const;

    virtual Qn::ActionScope currentScope() const override;

    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    QPalette comboBoxPalette() const;

    void setComboBoxPalette(const QPalette &palette);

signals:
    void activated(const QnResourcePtr &resource);
    void currentTabChanged();
    void selectionChanged();

protected:
    virtual void contextMenuEvent(QContextMenuEvent *) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void keyReleaseEvent(QKeyEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;

    QTreeView *currentItemView() const;
    QItemSelectionModel *currentSelectionModel() const;

    bool isLayoutSearchable(QnWorkbenchLayout *layout) const;
    QnResourceSearchProxyModel *layoutModel(QnWorkbenchLayout *layout, bool create) const;
    QnResourceSearchSynchronizer *layoutSynchronizer(QnWorkbenchLayout *layout, bool create) const;
    QString layoutFilter(QnWorkbenchLayout *layout) const;
    void setLayoutFilter(QnWorkbenchLayout *layout, const QString &filter) const;

    void killSearchTimer();
    void showContextMenuAt(const QPoint &pos);

private slots:
    void expandAll();

    void updateFilter(bool force = false);
    void forceUpdateFilter() { updateFilter(true); }
    
    void at_treeView_enterPressed(const QModelIndex &index);
    void at_treeView_doubleClicked(const QModelIndex &index);
    void at_tabWidget_currentChanged(int index);
    void at_resourceProxyModel_rowsInserted(const QModelIndex &parent, int start, int end);
    void at_resourceProxyModel_rowsInserted(const QModelIndex &index);

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    
    void at_workbench_itemChanged(Qn::ItemRole role);
    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);

private:
    QScopedPointer<Ui::ResourceTreeWidget> ui;

    bool m_ignoreFilterChanges;
    int m_filterTimerId;

    QnResourcePoolModel *m_resourceModel;
    QSortFilterProxyModel *m_resourceProxyModel;
    QnResourceTreeItemDelegate *m_resourceDelegate;
    QnResourceTreeItemDelegate *m_searchDelegate;

    QAction *m_renameAction;
};

#endif // QN_RESOURCE_TREE_WIDGET_H

#ifndef QN_RESOURCE_TREE_WIDGET_H
#define QN_RESOURCE_TREE_WIDGET_H

#include <QWidget>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <ui/actions/action_target_provider.h>

class QLineEdit;
class QTabWidget;
class QToolButton;
class QTreeView;
class QModelIndex;
class QItemSelectionModel;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;

class QnResourceCriterion;
class QnResourceModel;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizer;
class QnResourceTreeItemDelegate;

namespace Ui {
    class ResourceTreeWidget;
}

class QnResourceTreeWidget: public QWidget, public QnActionTargetProvider {
    Q_OBJECT

public:
    enum Tab {
        ResourcesTab,
        SearchTab,
        TabCount
    };

    QnResourceTreeWidget(QWidget *parent = 0);

    virtual ~QnResourceTreeWidget();

    Tab currentTab() const;

    void setCurrentTab(Tab tab);

    void setWorkbench(QnWorkbench *workbench);

    QnWorkbench *workbench() const {
        return m_workbench;
    }

    QnResourceList selectedResources() const;

    QnLayoutItemIndexList selectedLayoutItems() const;

    virtual Qn::ActionScope currentScope() const override;

    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    QPalette comboBoxPalette() const;

    void setComboBoxPalette(const QPalette &palette);

signals:
    void activated(const QnResourcePtr &resource);
    void currentTabChanged();

protected:
    virtual void contextMenuEvent(QContextMenuEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;

    QItemSelectionModel *currentSelectionModel() const;

    QnResourceSearchProxyModel *layoutModel(QnWorkbenchLayout *layout, bool create) const;
    QnResourceSearchSynchronizer *layoutSynchronizer(QnWorkbenchLayout *layout, bool create) const;
    QString layoutSearchString(QnWorkbenchLayout *layout) const;
    void setLayoutSearchString(QnWorkbenchLayout *layout, const QString &searchString) const;
    QnResourceCriterion *layoutCriterion(QnWorkbenchLayout *layout) const;
    void setLayoutCriterion(QnWorkbenchLayout *layout, QnResourceCriterion *criterion) const;

    void killSearchTimer();

private slots:
    void open();
    void updateFilter();
    
    void at_treeView_activated(const QModelIndex &index);
    void at_tabWidget_currentChanged(int index);

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    void at_workbench_aboutToBeDestroyed();

private:
    QScopedPointer<Ui::ResourceTreeWidget> ui;

    bool m_ignoreFilterChanges;
    int m_filterTimerId;

    QnResourceModel *m_resourceModel;
    QnResourceTreeItemDelegate *m_resourceDelegate;
    QnResourceTreeItemDelegate *m_searchDelegate;

    QnWorkbench *m_workbench;
};

#endif // QN_RESOURCE_TREE_WIDGET_H

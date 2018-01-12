#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <nx/client/desktop/ui/actions/action_target_provider.h>
#include <ui/common/tool_tip_queryable.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <client/client_globals.h>

#include <nx/utils/disconnect_helper.h>

class QComboBox;
class QLineEdit;
class QTabWidget;
class QToolButton;
class QTreeView;
class QModelIndex;
class QItemSelectionModel;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnWorkbenchContext;

class HoverFocusProcessor;

class QnResourcePreviewWidget;
class QnResourceTreeModel;
class QnResourceSearchSynchronizer;
class QnResourceTreeWidget;
class QnTextEditLabel;
class QnGraphicsToolTipWidget;
class QnCameraThumbnailManager;

namespace Ui {
    class ResourceBrowserWidget;
}

class QnResourceBrowserWidget:
    public QWidget,
    public QnWorkbenchContextAware,
    public nx::client::desktop::ui::action::TargetProvider,
    public ToolTipQueryable
{
    Q_OBJECT

public:
    enum Tab {
        ResourcesTab,
        SearchTab,
        TabCount
    };

    QnResourceBrowserWidget(QWidget* parent = nullptr, QnWorkbenchContext* context = nullptr);

    virtual ~QnResourceBrowserWidget();

    Tab currentTab() const;

    void setCurrentTab(Tab tab);

    void selectNodeByUuid(const QnUuid& id);
    void selectResource(const QnResourcePtr& resource);
    QnResourceList selectedResources() const;

    QnLayoutItemIndexList selectedLayoutItems() const;

    QnVideoWallItemIndexList selectedVideoWallItems() const;

    QnVideoWallMatrixIndexList selectedVideoWallMatrices() const;

    virtual nx::client::desktop::ui::action::ActionScope currentScope() const override;

    QComboBox* typeComboBox() const;

    virtual nx::client::desktop::ui::action::Parameters currentParameters(
        nx::client::desktop::ui::action::ActionScope scope) const override;

    void setToolTipParent(QGraphicsWidget* widget);

    bool isScrollBarVisible() const;

    void hideToolTip();
    void showToolTip();

    void clearSelection();

signals:
    void currentTabChanged();
    void selectionChanged();
    void scrollBarVisibleChanged();

protected:
    virtual void contextMenuEvent(QContextMenuEvent* ) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void keyReleaseEvent(QKeyEvent* event) override;
    virtual void timerEvent(QTimerEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

    virtual QString toolTipAt(const QPointF& pos) const override;
    virtual bool showOwnTooltip(const QPointF& pos) override;

private:
    QnResourceSearchQuery query() const;
    void setQuery(const QnResourceSearchQuery& query);

    QnResourceTreeWidget* currentTreeWidget() const;
    QItemSelectionModel* currentSelectionModel() const;
    QModelIndex itemIndexAt(const QPoint& pos) const;

    bool isLayoutSearchable(QnWorkbenchLayout* layout) const;
    QnResourceSearchProxyModel* layoutModel(QnWorkbenchLayout* layout, bool create) const;
    QnResourceSearchSynchronizer* layoutSynchronizer(QnWorkbenchLayout* layout, bool create) const;

    void killSearchTimer();
    void showContextMenuAt(const QPoint& pos, bool ignoreSelection = false);

    void handleItemActivated(const QModelIndex& index, bool withMouse);

    void setTooltipResource(const QnResourcePtr& camera);

    void selectIndices(const QModelIndexList& indices);

    void initNewSearch();

private slots:
    void updateFilter(bool force = false);
    void updateToolTipPosition();

    void updateIcons();

    void at_tabWidget_currentChanged(int index);

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();

    void at_workbench_itemChange(Qn::ItemRole role);
    void at_layout_itemAdded(QnWorkbenchItem* item);
    void at_layout_itemRemoved(QnWorkbenchItem* item);

    void at_thumbnailClicked();

private:
    QScopedPointer<Ui::ResourceBrowserWidget> ui;

    bool m_ignoreFilterChanges;
    int m_filterTimerId;

    QnResourceTreeModel* m_resourceModel;
    QnGraphicsToolTipWidget* m_tooltipWidget;
    HoverFocusProcessor* m_hoverProcessor;

    QMap<nx::client::desktop::ui::action::IDType, QAction*> m_renameActions;
    QnDisconnectHelperPtr m_disconnectHelper;

    QScopedPointer<QnCameraThumbnailManager> m_thumbnailManager;
    QnResourcePtr m_tooltipResource;
};

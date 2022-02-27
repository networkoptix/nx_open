// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/workbench/workbench_context_aware.h>

#include <client/client_globals.h>

#include <nx/utils/scoped_connections.h>

class QTreeView;
class QModelIndex;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnWorkbenchContext;

class HoverFocusProcessor;

class QnResourceSearchSynchronizer;
class QnResourceTreeWidget;
class VariantAnimator;

namespace nx::vms::client::desktop {

class DeprecatedThumbnailTooltip;
class CameraThumbnailManager;
class ResourceTreeIconDecoratorModel;
class ResourceTreeDragDropDecoratorModel;
class ResourceTreeInteractionHandler;
class GroupedResourcesViewStateKeeper;

namespace entity_item_model { class EntityItemModel; }
namespace entity_resource_tree { class ResourceTreeComposer; }

} // namespace nx::vms::client::desktop

namespace Ui { class ResourceBrowserWidget; }

class QnResourceBrowserWidget:
    public QWidget,
    public QnWorkbenchContextAware,
    public nx::vms::client::desktop::ui::action::TargetProvider
{
    Q_OBJECT
    Q_PROPERTY(int x READ x WRITE setX NOTIFY xChanged)

    using base_type = QWidget;

public:
    QnResourceBrowserWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~QnResourceBrowserWidget() override;

    int x() const;
    void setX(int x);

    void selectNodeByUuid(const QnUuid& id);
    void selectResource(const QnResourcePtr& resource);
    void clearSelection();

    virtual nx::vms::client::desktop::ui::action::ActionScope currentScope() const override;
    virtual nx::vms::client::desktop::ui::action::Parameters currentParameters(
        nx::vms::client::desktop::ui::action::ActionScope scope) const override;

    QTreeView* treeView() const;
    bool isScrollBarVisible() const;

    void hideToolTip();
    void showToolTip();

signals:
    void selectionChanged();
    void scrollBarVisibleChanged();
    void xChanged(int newX);
    void geometryChanged();

protected:
    virtual void contextMenuEvent(QContextMenuEvent* ) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void keyReleaseEvent(QKeyEvent* event) override;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    QnResourceTreeWidget* treeWidget() const;
    QModelIndex itemIndexAt(const QPoint& pos) const;

    void showContextMenuAt(const QPoint& pos, bool ignoreSelection = false);

    void initToolTip();
    void setTooltipResource(const QnResourcePtr& camera);
    void showOwnTooltip(const QPointF& pos);

    void selectIndices(const QModelIndexList& indices) const;

    /**
     * Resource Tree special "Select All" action implementation triggered by keyboard shortcut.
     *  - If current index is valid, all current index siblings with same node type are selected
     *  - Other already selected items are deselected
     *  - Current index doesn't affected
     */
    void onSelectAllActionTriggered();

    bool updateFilteringMode(bool value);
    void storeExpandedStates();
    void restoreExpandedStates();
    void setupAutoExpandPolicy();

    void initInstantSearch();
    void updateSearchMode();
    void updateInstantFilter();
    void handleInstantFilterUpdated();

    QMenu* createFilterMenu(QWidget* parent) const;
    QString getFilterName(nx::vms::client::desktop::ResourceTree::NodeType allowedNodeType) const;

    void updateTreeItem(const QnWorkbenchItem* item);

private slots:
    void updateIcons();

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    void at_workbench_itemChange(Qn::ItemRole role);

private:
    QScopedPointer<Ui::ResourceBrowserWidget> ui;

    QScopedPointer<nx::vms::client::desktop::entity_resource_tree::ResourceTreeComposer> m_resourceTreeComposer;
    QScopedPointer<nx::vms::client::desktop::entity_item_model::EntityItemModel> m_resourceModel;
    QScopedPointer<nx::vms::client::desktop::ResourceTreeDragDropDecoratorModel> m_dragDropDecoratorModel;
    QScopedPointer<nx::vms::client::desktop::ResourceTreeIconDecoratorModel> m_iconDecoratorModel;

    nx::vms::client::desktop::DeprecatedThumbnailTooltip* m_tooltip;
    VariantAnimator* m_opacityAnimator;
    QScopedPointer<HoverFocusProcessor> m_hoverProcessor;

    QScopedPointer<nx::vms::client::desktop::CameraThumbnailManager> m_thumbnailManager;
    QnResourcePtr m_tooltipResource;
    using ExpandedState = QPair<QPersistentModelIndex, bool>;
    QList<ExpandedState> m_expandedStatesList;

    QScopedPointer<nx::vms::client::desktop::ResourceTreeInteractionHandler> m_interactionHandler;
    QScopedPointer<nx::vms::client::desktop::GroupedResourcesViewStateKeeper> m_groupedResourcesViewState;

    bool m_filtering = false;

    nx::utils::ScopedConnections m_connections; //< Should be destroyed first.
};

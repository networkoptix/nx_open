#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <ui/common/tool_tip_queryable.h>
#include <ui/models/resource_search_proxy_model.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <client/client_globals.h>

#include <nx/utils/scoped_connections.h>

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

class QnResourceTreeModel;
class QnResourceSearchSynchronizer;
class QnResourceTreeWidget;
class TextEditLabel;
class QnGraphicsToolTipWidget;

namespace nx::vms::client::desktop { class CameraThumbnailManager; }

namespace Ui { class ResourceBrowserWidget; }

class QnResourceBrowserWidget:
    public QWidget,
    public QnWorkbenchContextAware,
    public nx::vms::client::desktop::ui::action::TargetProvider,
    public ToolTipQueryable
{
    Q_OBJECT

public:
    QnResourceBrowserWidget(QWidget* parent = nullptr, QnWorkbenchContext* context = nullptr);
    virtual ~QnResourceBrowserWidget() override;

    void selectNodeByUuid(const QnUuid& id);
    void selectResource(const QnResourcePtr& resource);
    QnResourceList selectedResources() const;

    QnLayoutItemIndexList selectedLayoutItems() const;

    QnVideoWallItemIndexList selectedVideoWallItems() const;

    QnVideoWallMatrixIndexList selectedVideoWallMatrices() const;

    virtual nx::vms::client::desktop::ui::action::ActionScope currentScope() const override;

    virtual nx::vms::client::desktop::ui::action::Parameters currentParameters(
        nx::vms::client::desktop::ui::action::ActionScope scope) const override;

    void setToolTipParent(QGraphicsWidget* widget);

    bool isScrollBarVisible() const;

    void hideToolTip();
    void showToolTip();

    void clearSelection();
signals:
    void selectionChanged();
    void scrollBarVisibleChanged();

protected:
    virtual void contextMenuEvent(QContextMenuEvent* ) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void keyReleaseEvent(QKeyEvent* event) override;

    virtual QString toolTipAt(const QPointF& pos) const override;
    virtual bool showOwnTooltip(const QPointF& pos) override;

private:
    QnResourceTreeWidget* currentTreeWidget() const;
    QItemSelectionModel* currentSelectionModel() const;
    QModelIndex itemIndexAt(const QPoint& pos) const;

    bool isLayoutSearchable(QnWorkbenchLayout* layout) const;
    QnResourceSearchProxyModel* layoutModel(QnWorkbenchLayout* layout, bool create) const;
    QnResourceSearchSynchronizer* layoutSynchronizer(QnWorkbenchLayout* layout, bool create) const;

    void showContextMenuAt(const QPoint& pos, bool ignoreSelection = false);

    void handleItemActivated(const QModelIndex& index, bool withMouse);

    void setTooltipResource(const QnResourcePtr& camera);

    void selectIndices(const QModelIndexList& indices);

    bool updateFilteringMode(bool value);
    void storeExpandedStates();
    void restoreExpandedStates();

    void initInstantSearch();
    void updateSearchMode();
    void updateInstantFilter();
    void handleInstantFilterUpdated();

    QMenu* createFilterMenu(QWidget* parent) const;
    QString getFilterName(nx::vms::client::desktop::ResourceTreeNodeType allowedNodeType) const;

    void setHintVisible(bool value);

    void setHintVisibleByBasicState(bool value);
    bool hintIsVisibleByBasicState() const;
    void updateHintVisibilityByBasicState();
    void handleEnterPressed(bool withControlKey);

    void setAvailableItemTypes(
        bool hasOpenInLayoutItems,
        bool hasOpenInEntityItems,
        bool hasUnopenableItems);
    bool hintIsVisbleByFilterState() const;
    void updateHintVisibilityByFilterState();

private slots:
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

    QnResourceTreeModel* m_resourceModel;
    QnGraphicsToolTipWidget* m_tooltipWidget;
    HoverFocusProcessor* m_hoverProcessor;

    QMap<nx::vms::client::desktop::ui::action::IDType, QAction*> m_renameActions;
    nx::utils::ScopedConnections m_connections;

    QScopedPointer<nx::vms::client::desktop::CameraThumbnailManager> m_thumbnailManager;
    QnResourcePtr m_tooltipResource;
    using ExpandedState = QPair<QPersistentModelIndex, bool>;
    QList<ExpandedState> m_expandedStatesList;

    bool m_filtering = false;


    bool m_hasOpenInLayoutItems = false;
    bool m_hasOpenInEntityItems = false;
    bool m_hasUnopenableItems = false;

    bool m_hintIsVisibleByBasicState = false;
};

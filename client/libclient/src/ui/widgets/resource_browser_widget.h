#ifndef QN_RESOURCE_BROWSER_WIDGET_H
#define QN_RESOURCE_BROWSER_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsProxyWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <ui/actions/action_target_provider.h>
#include <ui/common/tool_tip_queryable.h>
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

class QnResourceCriterion;
class QnResourcePreviewWidget;
class QnResourceTreeModel;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizer;
class QnResourceTreeWidget;
class QnCameraThumbnailManager;
class QnTextEditLabel;

namespace Ui {
    class ResourceBrowserWidget;
}

class QnResourceBrowserToolTipWidget: public QnStyledTooltipWidget
{
    Q_OBJECT
    using base_type = QnStyledTooltipWidget;

public:
    QnResourceBrowserToolTipWidget(QGraphicsItem* parent = nullptr);

    /**
     * Set tooltip text.
     * \param text                      New text for this tool tip's label.
     * \reimp
     */
    void setText(const QString& text);

    void setThumbnailVisible(bool visible);

    void setResource(const QnResourcePtr& resource);
    const QnResourcePtr& resource() const;

    //reimp
    void pointTo(const QPointF& pos);
    virtual void updateTailPos() override;

signals:
    void thumbnailClicked();

protected:
    virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;

private:
    void forceLayoutUpdate();

private:
    QGraphicsProxyWidget* m_proxyWidget;
    QWidget* m_embeddedWidget;
    QnTextEditLabel* m_textLabel;
    QnResourcePreviewWidget* m_previewWidget;
    QPointF m_pointTo;
};


class QnResourceBrowserWidget: public QWidget, public QnWorkbenchContextAware, public QnActionTargetProvider, public ToolTipQueryable {
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

    QnResourceList selectedResources() const;

    QnLayoutItemIndexList selectedLayoutItems() const;

    QnVideoWallItemIndexList selectedVideoWallItems() const;

    QnVideoWallMatrixIndexList selectedVideoWallMatrices() const;

    virtual Qn::ActionScope currentScope() const override;

    QComboBox* typeComboBox() const;

    virtual QnActionParameters currentParameters(Qn::ActionScope scope) const override;

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
    QnResourceTreeWidget* currentTreeWidget() const;
    QItemSelectionModel* currentSelectionModel() const;
    QModelIndex itemIndexAt(const QPoint& pos) const;

    bool isLayoutSearchable(QnWorkbenchLayout* layout) const;
    QnResourceSearchProxyModel* layoutModel(QnWorkbenchLayout* layout, bool create) const;
    QnResourceSearchSynchronizer* layoutSynchronizer(QnWorkbenchLayout* layout, bool create) const;
    QString layoutFilter(QnWorkbenchLayout* layout) const;
    void setLayoutFilter(QnWorkbenchLayout* layout, const QString& filter) const;

    void killSearchTimer();
    void showContextMenuAt(const QPoint& pos, bool ignoreSelection = false);

    void setupInitialModelCriteria(QnResourceSearchProxyModel* model) const;

    void handleItemActivated(const QModelIndex& index, bool withMouse);

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
    QnResourceBrowserToolTipWidget* m_tooltipWidget;
    HoverFocusProcessor* m_hoverProcessor;

    QMap<QnActions::IDType, QAction*> m_renameActions;
    QnDisconnectHelperPtr m_disconnectHelper;
};

#endif // QN_RESOURCE_BROWSER_WIDGET_H

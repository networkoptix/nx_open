#ifndef QN_RESOURCE_BROWSER_WIDGET_H
#define QN_RESOURCE_BROWSER_WIDGET_H

#include <QWidget>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <ui/actions/action_target_provider.h>
#include <ui/workbench/workbench_context_aware.h>

#include <ui/graphics/items/generic/styled_tooltip_widget.h>

#include <client/client_globals.h>

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

class QnImageProvider;
class QnProxyLabel;
class QnClickableProxyLabel;
class HoverFocusProcessor;

class QnResourceCriterion;
class QnResourcePoolModel;
class QnResourceSearchProxyModel;
class QnResourceSearchSynchronizer;
class QnResourceTreeWidget;

namespace Ui {
    class ResourceBrowserWidget;
}

class QnResourceBrowserToolTipWidget: public QnStyledTooltipWidget {
    Q_OBJECT

    typedef QnStyledTooltipWidget base_type;
public:
    QnResourceBrowserToolTipWidget(QGraphicsItem *parent = 0);

    void ensureThumbnail(QnImageProvider* provider);

    /**
     * Set tooltip text.
     * \param text                      New text for this tool tip's label.
     * \reimp
     */
    void setText(const QString &text);

    /** Rectangle where the tooltip should fit - in coordinates of parent widget. */
    void setEnclosingGeometry(const QRectF &enclosingGeometry);

    //reimp
    void pointTo(const QPointF &pos);
    virtual void updateTailPos() override;
signals:
    void thumbnailClicked();

private slots:
    void at_provider_imageChanged(const QImage &image);

private:
    QnProxyLabel* m_textLabel;
    QnClickableProxyLabel* m_thumbnailLabel;
    QRectF m_enclosingRect;
    QPointF m_pointTo;
};


class QnResourceBrowserWidget: public QWidget, public QnWorkbenchContextAware, public QnActionTargetProvider {
    Q_OBJECT

public:
    enum Tab {
        ResourcesTab,
        SearchTab,
        TabCount
    };

    QnResourceBrowserWidget(QWidget *parent = NULL, QnWorkbenchContext *context = NULL);

    virtual ~QnResourceBrowserWidget();

    Tab currentTab() const;

    void setCurrentTab(Tab tab);

    QnResourceList selectedResources() const;

    QnLayoutItemIndexList selectedLayoutItems() const;

    virtual Qn::ActionScope currentScope() const override;

    QComboBox *typeComboBox() const;

    virtual QnActionParameters currentParameters(Qn::ActionScope scope) const override;

    void setToolTipParent(QGraphicsWidget *widget);

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

    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    QnResourceTreeWidget *currentItemView() const;
    QItemSelectionModel *currentSelectionModel() const;

    bool isLayoutSearchable(QnWorkbenchLayout *layout) const;
    QnResourceSearchProxyModel *layoutModel(QnWorkbenchLayout *layout, bool create) const;
    QnResourceSearchSynchronizer *layoutSynchronizer(QnWorkbenchLayout *layout, bool create) const;
    QString layoutFilter(QnWorkbenchLayout *layout) const;
    void setLayoutFilter(QnWorkbenchLayout *layout, const QString &filter) const;

    void killSearchTimer();
    void showContextMenuAt(const QPoint &pos, bool ignoreSelection = false);

    void hideToolTip();
    void showToolTip();
private slots:
    void updateFilter(bool force = false);
    void updateToolTipVisibility();
    void updateToolTipPosition();

    void forceUpdateFilter() { updateFilter(true); }

    void at_tabWidget_currentChanged(int index);

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    
    void at_workbench_itemChanged(Qn::ItemRole role);
    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);

    void at_showUrlsInTree_changed();

private:
    QScopedPointer<Ui::ResourceBrowserWidget> ui;

    bool m_ignoreFilterChanges;
    int m_filterTimerId;

    QnResourcePoolModel *m_resourceModel;
    QnResourceBrowserToolTipWidget* m_tooltipWidget;
    HoverFocusProcessor* m_hoverProcessor;

    QAction *m_renameAction;
};

#endif // QN_RESOURCE_BROWSER_WIDGET_H

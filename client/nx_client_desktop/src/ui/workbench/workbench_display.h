#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtOpenGL/QGLWidget>

#include <nx/vms/event/event_fwd.h>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/common/scene_transformations.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/graphics/view/graphics_view.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>

#include <utils/common/connective.h>

class QGraphicsScene;
class QGraphicsView;

class InstrumentManager;
class BoundingInstrument;
class TransformListenerInstrument;
class ActivityListenerInstrument;
class ForwardingInstrument;
class SignalingInstrument;
class SelectionOverlayHackInstrument;
class FocusListenerInstrument;

class QnWorkbench;
class QnWorkbenchItem;

class QnWorkbenchLayout;
class QnResourceWidget;
class QnResourceDisplay;
typedef QSharedPointer<QnResourceDisplay> QnResourceDisplayPtr;
class ViewportAnimator;
class WidgetAnimator;
class QnCurtainAnimator;
class QnCurtainItem;
class QnGridItem;
class QnGridBackgroundItem;
class QnThumbnailsLoader;
class QnThumbnail;

class QnClientVideoCamera;
class QnCamDisplay;

/**
 * This class ties a workbench, a scene and a view together.
 *
 * It presents some low-level functions for viewport and item manipulation.
 */
class QnWorkbenchDisplay:
    public Connective<QObject>,
    public QnWorkbenchContextAware,
    protected QnSceneTransformations
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:

    /**
    * Layer of a graphics item on the scene.
    *
    * Workbench display presents convenience functions for moving items between layers
    * and guarantees that items from the layers with higher numbers are always
    * displayed on top of those from the layers with lower numbers.
    */
    enum ItemLayer
    {
        InvisibleLayer = -1,        //< Layer for invisible items.
        EMappingLayer = 0,          /**< Layer for E-Mapping background. */
        BackLayer,                  /**< Back layer. */
        PinnedLayer,                /**< Layer for pinned items. */
        PinnedRaisedLayer,          /**< Layer for pinned items that are raised. */
        UnpinnedLayer,              /**< Layer for unpinned items. */
        UnpinnedRaisedLayer,        /**< Layer for unpinned items that are raised. */
        ZoomedLayer,                /**< Layer for zoomed items. */
        FrontLayer,                 /**< Topmost layer for items. Items that are being dragged, resized or manipulated in any other way are to be placed here. */
        EffectsLayer,               /**< Layer for top-level effects. */
        UiLayer,                    /**< Layer for ui elements, i.e. navigation bar, resource tree, etc... */
        MessageBoxLayer,            /**< Layer for graphics text messages. */
        LayerCount
    };

    /**
     * Constructor.
     *
     * \param parent                    Parent object for this workbench display.
     */
    QnWorkbenchDisplay(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchDisplay();

    /**
    * \returns                         Light mode of this workbench display.
    */
    Qn::LightModeFlags lightMode() const;

    /**
    * \param mode                      Light mode for the current display.
    *                                  Enables or disables certain visualization features
    *                                  to simplify and speed up painting.
    */
    void setLightMode(Qn::LightModeFlags mode);

    /**
     * \returns                         Instrument manager owned by this workbench display.
     */
    InstrumentManager* instrumentManager() const;

    /**
     * \returns                         Bounding instrument used by this workbench display.
     */
    BoundingInstrument* boundingInstrument() const;

    /**
     * \returns                         Transformation listener instrument used by this workbench display.
     */
    TransformListenerInstrument* transformationListenerInstrument() const;

    FocusListenerInstrument* focusListenerInstrument() const;

    /**
     * \returns                         Paint forwarding instrument used by this workbench display.
     */
    ForwardingInstrument* paintForwardingInstrument() const;

    SelectionOverlayHackInstrument* selectionOverlayHackInstrument() const;

    SignalingInstrument* beforePaintInstrument() const;

    SignalingInstrument* afterPaintInstrument() const;


    /**
     * Note that this function never returns NULL.
     *
     * \returns                         Current scene of this workbench display.
     */
    QGraphicsScene* scene() const;

    /**
     * Note that workbench manager does not take ownership of the supplied scene.
     *
     * \param scene                     New scene for this workbench display.
     *                                  If NULL is supplied, an empty scene
     *                                  owned by this workbench display is used.
     */
    void setScene(QGraphicsScene* scene);

    /**
     * \returns                         Current graphics view of this workbench display.
     *                                  May be NULL.
     */
    QnGraphicsView* view() const;

    /**
     * \param view                      New view for this workbench display.
     */
    void setView(QnGraphicsView* view);

    /**
     * \returns                         Grid item.
     */
    QnGridItem *gridItem() const;

    /**
     * \returns                         Curtain item (that is painted in black when a single widget is zoomed).
     */
    QnCurtainItem* curtainItem() const;

    /**
     * \returns                         Curtain item animator.
     */
    QnCurtainAnimator* curtainAnimator() const;

    /**
     * \returns                         Grid background item (E-Mapping).
     */
    QnGridBackgroundItem *gridBackgroundItem() const;

    /**
     * \param item                      Item to get widget for.
     * \returns                         Widget for the given item.
     */
    QnResourceWidget *widget(QnWorkbenchItem *item) const;

    QnResourceWidget *widget(Qn::ItemRole role) const;

    QnResourceWidget *widget(const QnUuid &uuid) const;

    QList<QnResourceWidget *> widgets(const QnResourcePtr &resource) const;

    QList<QnResourceWidget *> widgets() const;

    QnResourceWidget* activeWidget() const;

    QnResourceDisplayPtr display(QnWorkbenchItem *item) const;

    QnClientVideoCamera *camera(QnWorkbenchItem *item) const;

    QnCamDisplay *camDisplay(QnWorkbenchItem *item) const;

    /**
     * \param item                      Item to get enclosing geometry for.
     * \returns                         Given item's enclosing geometry in scene
     *                                  coordinates as defined by the model.
     *                                  Note that actual geometry may differ because of
     *                                  aspect ration constraints.
     */
    QRectF itemEnclosingGeometry(QnWorkbenchItem *item) const;

    /**
     * \param item                      Item to get geometry for.
     * \param[out] enclosingGeometry    Item's enclosing geometry.
     * \returns                         Item's geometry in scene coordinates,
     *                                  taking aspect ratio constraints into account.
     *                                  Note that actual geometry of the item's widget
     *                                  may differ because of manual dragging / resizing / etc...
     */
    QRectF itemGeometry(QnWorkbenchItem *item, QRectF *enclosingGeometry = NULL) const;

    QRectF fitInViewGeometry() const;

    /**
     * \returns                         Current viewport geometry, in scene coordinates.
     */
    QRectF viewportGeometry() const;

    /**
     * \returns                         Current viewport geometry, in scene coordinates, calculated taking viewport margins into account.
     */
    QRectF boundedViewportGeometry(Qn::MarginTypes marginTypes= Qn::CombinedMargins) const;

    /**
     * This function can be used in case the "actual" viewport differs from the
     * "real" one. This can be the case if control panels are drawn on the scene.
     * Some layouts can have additional viewport margins, that are changed independently of panels.
     * For example, these are tour review layouts. Different fields are required to correctly
     * calculate background rect.
     *
     * \param margins                   New viewport margins.
     * \param marginType                Type of margins to be set.
     */
    void setViewportMargins(const QMargins& margins, Qn::MarginType marginType);

    QMargins viewportMargins(Qn::MarginTypes marginTypes = Qn::CombinedMargins) const;

    Qn::MarginFlags currentMarginFlags() const;

    Qn::MarginFlags zoomedMarginFlags() const;

    Qn::MarginFlags normalMarginFlags() const;

    void setZoomedMarginFlags(Qn::MarginFlags flags);

    void setNormalMarginFlags(Qn::MarginFlags flags);


    void bringToFront(const QList<QGraphicsItem *> &items);

    void bringToFront(QGraphicsItem *item);

    void bringToFront(QnWorkbenchItem *item);

    ItemLayer layer(QGraphicsItem *item) const;
    void setLayer(QGraphicsItem *item, ItemLayer layer);
    void setLayer(const QList<QGraphicsItem *> &items, ItemLayer layer);
    qreal layerZValue(ItemLayer layer) const;

    void synchronize(QnWorkbenchItem *item, bool animate);

    void synchronize(QnResourceWidget *widget, bool animate);


    QPoint mapViewportToGrid(const QPoint &viewportPoint) const;

    QPoint mapGlobalToGrid(const QPoint &globalPoint) const;

    QPointF mapViewportToGridF(const QPoint &viewportPoint) const;

    QPointF mapGlobalToGridF(const QPoint &globalPoint) const;

    /**
     * Status function to know if we are changing layout now.
     * \returns true if we are changing layout
     */
    bool isChangingLayout() const { return m_inChangeLayout; } // TODO: #GDM this is evil

    QnResourceWidget *zoomTargetWidget(QnResourceWidget *widget) const;

    QRectF raisedGeometry(const QRectF &widgetGeometry, qreal rotation) const;

    QGLWidget *newGlWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) const;

    QSet<QnWorkbenchItem*> draggedItems() const;
    void setDraggedItems(const QSet<QnWorkbenchItem*>& value);

    bool animationAllowed() const;

public slots:
    void fitInView(bool animate);

signals:
    void viewportGrabbed();
    void viewportUngrabbed();

    void widgetAdded(QnResourceWidget *widget);
    void widgetAboutToBeRemoved(QnResourceWidget *widget);
    void widgetChanged(Qn::ItemRole role);

    void zoomLinkAdded(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    void zoomLinkAboutToBeRemoved(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);

    void resourceAdded(const QnResourcePtr &resource);
    void resourceAboutToBeRemoved(const QnResourcePtr &resource);

protected:
    WidgetAnimator *animator(QnResourceWidget *widget);

    void synchronizeGeometry(QnWorkbenchItem *item, bool animate);
    void synchronizeGeometry(QnResourceWidget *widget, bool animate);
    void synchronizeZoomRect(QnWorkbenchItem *item);
    void synchronizeZoomRect(QnResourceWidget *widget);
    void synchronizeAllGeometries(bool animate);
    void synchronizeLayer(QnWorkbenchItem *item);
    void synchronizeLayer(QnResourceWidget *widget);
    void synchronizePlaceholder(QnResourceWidget *widget);
    void synchronizeSceneBounds();

    void updateCurrentMarginFlags();

    void adjustGeometryLater(QnWorkbenchItem *item, bool animate);
    void adjustGeometry(QnWorkbenchItem *item, bool animate);

    qreal layerFrontZValue(ItemLayer layer) const;
    ItemLayer synchronizedLayer(QnResourceWidget *widget) const;
    ItemLayer shadowLayer(ItemLayer itemLayer) const;

    bool addItemInternal(QnWorkbenchItem *item, bool animate, bool startDisplay);
    bool removeItemInternal(QnWorkbenchItem *item, bool destroyWidget, bool destroyItem);

    bool addZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    bool removeZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    bool addZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    bool removeZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    bool removeZoomLinksInternal(QnWorkbenchItem *item);

    void deinitSceneView();
    void initSceneView();
    void initContext(QnWorkbenchContext *context);
    void initBoundingInstrument();

    void setWidget(Qn::ItemRole role, QnResourceWidget *widget);

protected slots:
    void synchronizeSceneBoundsExtension();
    void synchronizeRaisedGeometry();

    void updateBackground(const QnLayoutResourcePtr &layout);

    /** Mark item on the scene selected as it was selected in the tree. */
    void updateSelectionFromTree();

    void at_scene_destroyed();
    void at_scene_selectionChanged();

    void at_viewportAnimator_finished();

    void at_workbench_itemChanged(Qn::ItemRole role, QnWorkbenchItem *item);
    void at_workbench_itemChanged(Qn::ItemRole role);
    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();

    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);
    void at_layout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_layout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_layout_boundingRectChanged(const QRect &oldRect, const QRect &newRect);

    void at_context_permissionsChanged(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);

    void at_item_geometryChanged();
    void at_item_geometryDeltaChanged();
    void at_item_zoomRectChanged();
    void at_item_rotationChanged();
    void at_item_dataChanged(Qn::ItemDataRole role);
    void at_item_flagChanged(Qn::ItemFlag flag, bool value);

    void at_widgetActivityInstrument_activityStopped();
    void at_widgetActivityInstrument_activityStarted();

    void at_widget_aspectRatioChanged();
    void at_widget_aboutToBeDestroyed();

    void at_view_destroyed();

    void at_mapper_originChanged();
    void at_mapper_cellSizeChanged();
    void at_mapper_spacingChanged();

    void at_previewSearch_thumbnailLoaded(const QnThumbnail &thumbnail);

    void at_notificationsHandler_businessActionAdded(const nx::vms::event::AbstractActionPtr &businessAction);
    void showSplashOnResource(const QnResourcePtr &resource, const nx::vms::event::AbstractActionPtr &businessAction);

    bool canShowLayoutBackground() const;
private:
    /* Directly visible state */

    /** Current scene. */
    QGraphicsScene *m_scene;

    /** Current view. */
    QnGraphicsView *m_view;

    /** Set of flags to simplify and speed up painting. */
    Qn::LightModeFlags m_lightMode;

    /* Internal state. */
    QList<QnResourceWidget *> m_widgets;

    /** Item to widget mapping. */
    QHash<QnWorkbenchItem *, QnResourceWidget *> m_widgetByItem;

    /** Resource to widget mapping. */
    QHash<QnResourcePtr, QList<QnResourceWidget *> > m_widgetsByResource;

    /** Widget to zoom target widget mapping. */
    QHash<QnResourceWidget *, QnResourceWidget *> m_zoomTargetWidgetByWidget;

    /** Current front z displacement value. */
    qreal m_frontZ;

    /** Current items by role. */
    std::array<QnResourceWidget*, Qn::ItemRoleCount> m_widgetByRole;

    /** Grid item. */
    QPointer<QnGridItem> m_gridItem;

    /** Grid background item. */
    QPointer<QnGridBackgroundItem> m_gridBackgroundItem;

    Qn::MarginFlags m_zoomedMarginFlags, m_normalMarginFlags;

    /** Whether we are changing layout now. */
    bool m_inChangeLayout;

    QSet<QnWorkbenchItem*> m_draggedItems;


    /* Instruments. */

    /** Instrument manager owned by this workbench manager. */
    InstrumentManager *m_instrumentManager;

    /** Transformation listener instrument. */
    TransformListenerInstrument *m_transformListenerInstrument;

    /** Activity listener instrument for resource widgets. */
    ActivityListenerInstrument *m_widgetActivityInstrument;

    /** Focus listener instrument. */
    FocusListenerInstrument *m_focusListenerInstrument;

    /** Bounding instrument. */
    BoundingInstrument *m_boundingInstrument;

    /** Paint forwarding instrument. */
    ForwardingInstrument *m_paintForwardingInstrument;

    /** Selection overlay hack instrument. */
    SelectionOverlayHackInstrument *m_selectionOverlayHackInstrument;

    SignalingInstrument *m_beforePaintInstrument;

    SignalingInstrument *m_afterPaintInstrument;


    /* Animation-related stuff. */

    /** Viewport animator. */
    ViewportAnimator *m_viewportAnimator;

    /** Curtain item. */
    QPointer<QnCurtainItem> m_curtainItem;

    /** Curtain animator. */
    QnCurtainAnimator *m_curtainAnimator;

    QnThumbnailsLoader *m_loader;
};

#ifndef QN_DROP_INSTRUMENT_H
#define QN_DROP_INSTRUMENT_H

#include "instrument.h"

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item_index.h>
#include "scene_event_filter.h"

class QnWorkbenchContext;
class DropSurfaceItem;
class DestructionGuardItem;

class DropInstrument: public Instrument, public SceneEventFilter {
    Q_OBJECT
public:
    DropInstrument(bool intoNewLayout, QnWorkbenchContext *context, QObject *parent = NULL);

    /**
     * \returns                         Graphics item that serves as a surface for 
     *                                  drag and drop. Is never NULL, unless explicitly deleted by user.
     */
    QGraphicsObject *surface() const;

    void setSurface(QGraphicsObject *surface);

protected:
    friend class DropSurfaceItem;

    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;

    virtual bool dragEnterEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dragMoveEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dragLeaveEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dropEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;

    DestructionGuardItem *guard() const;
    SceneEventFilterItem *filterItem() const;

private:
    QnResourceList m_resources;
    QnVideoWallItemIndexList m_videoWallItems;
    
    QPointer<QnWorkbenchContext> m_context;
    QScopedPointer<SceneEventFilterItem> m_filterItem;
    QPointer<DestructionGuardItem> m_guard;
    QPointer<QGraphicsObject> m_surface;
    bool m_intoNewLayout;
};

/** Name of the mimetype to add to a drag object to forbid its dropping on the scene. */
#define NoSceneDrop _id(lit("_qn_noSceneDrop"))

#endif // QN_DROP_INSTRUMENT_H

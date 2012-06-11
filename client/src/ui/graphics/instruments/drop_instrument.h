#ifndef QN_DROP_INSTRUMENT_H
#define QN_DROP_INSTRUMENT_H

#include "instrument.h"
#include <core/resource/resource.h>
#include "scene_event_filter.h"

class QnWorkbenchContext;
class DropSurfaceItem;
class DestructionGuardItem;

class DropInstrument: public Instrument, public SceneEventFilter {
    Q_OBJECT;
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

    DestructionGuardItem *guard() const {
        return m_guard.data();
    }

    SceneEventFilterItem *filterItem() const {
        return m_filterItem.data();
    }

private:
    QnResourceList m_resources;
    
    QWeakPointer<QnWorkbenchContext> m_context;
    QScopedPointer<SceneEventFilterItem> m_filterItem;
    QWeakPointer<DestructionGuardItem> m_guard;
    QWeakPointer<QGraphicsObject> m_surface;
    bool m_intoNewLayout;
};

#endif // QN_DROP_INSTRUMENT_H

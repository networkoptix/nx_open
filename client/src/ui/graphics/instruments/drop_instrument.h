#ifndef QN_DROP_INSTRUMENT_H
#define QN_DROP_INSTRUMENT_H

#include "instrument.h"
#include <core/resource/resource.h>

class QnWorkbenchContext;
class DropSurfaceItem;
class DestructionGuardItem;

class DropInstrument: public Instrument {
    Q_OBJECT;
public:
    DropInstrument(QnWorkbenchContext *context, QObject *parent = NULL);

    /**
     * \returns                         Graphics item that serves as a surface for 
     *                                  drag and drop. Is never NULL, unless explicitly deleted by user.
     */
    QGraphicsObject *surface() const;

protected:
    friend class DropSurfaceItem;

    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool dragEnterEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dragMoveEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dragLeaveEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dropEvent(QGraphicsItem *item, QGraphicsSceneDragDropEvent *event) override;

    DestructionGuardItem *guard() const {
        return m_guard.data();
    }

private:
    bool dropInternal(QGraphicsView *view, const QStringList &files, const QPoint &pos);

private:
    QStringList m_files;
    QnResourceList m_resources;
    
    QWeakPointer<QnWorkbenchContext> m_context;
    QWeakPointer<DropSurfaceItem> m_surface;
    QWeakPointer<DestructionGuardItem> m_guard;
};

#endif // QN_DROP_INSTRUMENT_H

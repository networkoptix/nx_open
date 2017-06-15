#pragma once

#include "instrument.h"
#include "scene_event_filter.h"

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item_index.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

class QnWorkbenchContext;
class DropSurfaceItem;
class DestructionGuardItem;

class DropInstrument: public Instrument, public SceneEventFilter, public QnConnectionContextAware
{
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
    bool delayedTriggerIfPossible(nx::client::desktop::ui::action::IDType id,
        const nx::client::desktop::ui::action::Parameters& parameters);

private:
    QList<QnUuid> m_ids;

    QPointer<QnWorkbenchContext> m_context;
    QScopedPointer<SceneEventFilterItem> m_filterItem;
    QPointer<DestructionGuardItem> m_guard;
    QPointer<QGraphicsObject> m_surface;
    bool m_intoNewLayout;
};

/** Name of the mimetype to add to a drag object to forbid its dropping on the scene. */
#define NoSceneDrop _id(lit("_qn_noSceneDrop"))

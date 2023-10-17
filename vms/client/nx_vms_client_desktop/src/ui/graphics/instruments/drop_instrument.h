// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item_index.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <ui/workbench/workbench_context_aware.h>

#include "instrument.h"
#include "scene_event_filter.h"

class QnWorkbenchContext;
class DropSurfaceItem;
class DestructionGuardItem;

namespace nx::vms::client::desktop {

class MimeData;

} // namespace nx::vms::client::desktop

class DropInstrument:
    public Instrument,
    public SceneEventFilter,
    public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    DropInstrument(bool intoNewLayout, QnWorkbenchContext *context, QObject *parent = nullptr);
    virtual ~DropInstrument() override;

    /**
     * \returns                         Graphics item that serves as a surface for
     *                                  drag and drop. Is never nullptr, unless explicitly deleted by user.
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
    bool delayedTriggerIfPossible(nx::vms::client::desktop::menu::IDType id,
        const nx::vms::client::desktop::menu::Parameters& parameters);

    bool isDragValid() const;

private:
    std::unique_ptr<nx::vms::client::desktop::MimeData> m_mimeData;

    QScopedPointer<SceneEventFilterItem> m_filterItem;
    QPointer<DestructionGuardItem> m_guard;
    QPointer<QGraphicsObject> m_surface;
    bool m_intoNewLayout;
};

/** Name of the mimetype to add to a drag object to forbid its dropping on the scene. */
#define NoSceneDrop _id(lit("_qn_noSceneDrop"))

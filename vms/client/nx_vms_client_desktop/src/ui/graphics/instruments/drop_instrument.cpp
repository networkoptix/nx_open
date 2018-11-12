#include "drop_instrument.h"

#include <limits>

#include <QtCore/QMimeData>
#include <QtCore/QFile>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneDragDropEvent>
#include <QtWidgets/QGraphicsItem>

#include <common/common_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/file_processor.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <nx/utils/scope_guard.h>

#include <utils/common/delayed.h>
#include <utils/common/warnings.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workaround/mac_utils.h>

#include "destruction_guard_item.h"

#include <nx/vms/client/desktop/utils/mime_data.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

class DropSurfaceItem: public QGraphicsObject {
public:
    DropSurfaceItem(QGraphicsItem *parent = NULL):
        QGraphicsObject(parent)
    {
        qreal d = std::numeric_limits<qreal>::max() / 4;
        m_boundingRect = QRectF(QPointF(-d, -d), QPointF(d, d));

        setAcceptedMouseButtons(0);
        setAcceptDrops(true);
        /* Don't disable this item here or it will swallow mouse wheel events. */
    }

    virtual QRectF boundingRect() const override {
        return m_boundingRect;
    }

    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
        return;
    }

private:
    QRectF m_boundingRect;
};


DropInstrument::DropInstrument(bool intoNewLayout, QnWorkbenchContext *context, QObject *parent):
    Instrument(Item, makeSet(/* No events here, we'll receive them from the surface item. */), parent),
    m_context(context),
    m_filterItem(new SceneEventFilterItem()),
    m_intoNewLayout(intoNewLayout)
{
    if(context == NULL)
        qnNullWarning(context);

    m_filterItem->setEventFilter(this);
}

DropInstrument::~DropInstrument()
{
}

QGraphicsObject *DropInstrument::surface() const
{
    return m_surface.data();
}

void DropInstrument::setSurface(QGraphicsObject *surface) {
    if(this->surface()) {
        this->surface()->removeSceneEventFilter(filterItem());

        if(this->surface()->parent() == this)
            delete this->surface();
    }

    m_surface = surface;

    if(this->surface()) {
        this->surface()->setAcceptDrops(true);
        this->surface()->installSceneEventFilter(filterItem());
    }
}

void DropInstrument::installedNotify() {
    DestructionGuardItem *guard = new DestructionGuardItem();
    guard->setGuarded(filterItem());
    guard->setPos(0.0, 0.0);
    scene()->addItem(guard);

    if(surface() == NULL) {
        DropSurfaceItem *surface = new DropSurfaceItem();
        surface->setParent(this);
        scene()->addItem(surface);
        setSurface(surface);
    }

    m_guard = guard;
}

void DropInstrument::aboutToBeUninstalledNotify() {
    if(guard() != NULL)
        delete guard();

    setSurface(NULL);
}

bool DropInstrument::sceneEventFilter(QGraphicsItem *watched, QEvent *event) {
    return this->sceneEvent(watched, event);
}

bool DropInstrument::dragEnterEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *event)
{
    m_mimeData.reset();

    const auto mimeData = event->mimeData();
    if (mimeData->hasFormat(Qn::NoSceneDrop))
    {
        event->ignore();
        return false;
    }

    m_mimeData.reset(new MimeData{mimeData, resourcePool()});

    if (!isDragValid())
    {
        event->ignore();
        return false;
    }

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragMoveEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *event)
{
    if (!isDragValid())
    {
        event->ignore();
        return false;
    }

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragLeaveEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *)
{
    m_mimeData.reset();
    return true;
}

bool DropInstrument::dropEvent(QGraphicsItem* /*item*/, QGraphicsSceneDragDropEvent* event)
{
    // Reset stored mimedata.
    std::unique_ptr<nx::vms::client::desktop::MimeData> localMimeData = std::move(m_mimeData);

    auto context = m_context.data();
    if (!context)
        return true;

    const auto mimeData = event->mimeData();
    if (mimeData->hasFormat(Qn::NoSceneDrop))
    {
        NX_ASSERT(false, "Mimedata format could not be changed during drag");
        return false;
    }

    event->acceptProposedAction();

    const auto layoutTours = layoutTourManager()->tours(localMimeData->entities());
    if (!layoutTours.empty())
    {
        for (const auto& tour : layoutTours)
            delayedTriggerIfPossible(action::ReviewLayoutTourAction, {Qn::UuidRole, tour.id});

        // If tour was opened, ignore other items.
        return true;
    }

    // Try to drop videowall items first.
    const auto videoWallItems = resourcePool()->getVideoWallItemsByUuid(localMimeData->entities());
    if (delayedTriggerIfPossible(action::StartVideoWallControlAction, videoWallItems))
    {
        // Ignore resources.
        return true;
    }

    if (localMimeData->resources().empty())
        return false;

    resourcePool()->addNewResources(localMimeData->resources());

    if (!m_intoNewLayout)
    {
        delayedTriggerIfPossible(
            action::DropResourcesAction,
            action::Parameters(localMimeData->resources()).withArgument(Qn::ItemPositionRole,
                context->workbench()->mapper()->mapToGridF(event->scenePos())));
    }
    else
    {
        delayedTriggerIfPossible(action::OpenInNewTabAction, localMimeData->resources());
    }

    return true;
}

DestructionGuardItem *DropInstrument::guard() const {
    return m_guard.data();
}

SceneEventFilterItem *DropInstrument::filterItem() const {
    return m_filterItem.data();
}

bool DropInstrument::delayedTriggerIfPossible(action::IDType id, const action::Parameters& parameters)
{
    if (!m_context || !m_context->menu()->canTrigger(id, parameters))
        return false;

    const auto triggerIfPossible =
        [context = m_context, id, parameters]()
        {
            if (!context)
                return;

            QApplication::setOverrideCursor(Qt::WaitCursor);
            auto cursorGuard = nx::utils::makeScopeGuard(
                []() { QApplication::restoreOverrideCursor(); });

            context->menu()->triggerIfPossible(id, parameters);
        };

    executeDelayed(triggerIfPossible);
    return true;
}

bool DropInstrument::isDragValid() const
{
    return !m_mimeData->isEmpty();
}

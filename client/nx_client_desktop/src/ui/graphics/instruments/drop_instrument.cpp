#include "drop_instrument.h"

#include <limits>

#include <QtCore/QMimeData>
#include <QtCore/QFile>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneDragDropEvent>
#include <QtWidgets/QGraphicsItem>

#include <common/common_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/file_processor.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <nx/utils/raii_guard.h>

#include <utils/common/delayed.h>
#include <utils/common/warnings.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workaround/mac_utils.h>

#include <nx/client/desktop/utils/mime_data.h>

#include "destruction_guard_item.h"

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

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

QGraphicsObject *DropInstrument::surface() const {
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
    m_ids.clear();

    const auto mimeData = event->mimeData();
    if (mimeData->hasFormat(Qn::NoSceneDrop))
    {
        event->ignore();
        return false;
    }

#ifdef Q_OS_MAC
    if (mimeData->hasUrls())
    {
        foreach(const QUrl &url, mimeData->urls())
        {
            if (url.isLocalFile() && QFile::exists(url.toLocalFile()))
                mac_saveFileBookmark(url.path());
        }
    }
#endif

    // Scene drop is working only for resources that are already in the pool.
    MimeData data(mimeData);
    m_ids = data.getIds();

    if (m_ids.empty())
    {
        event->ignore();
        return false;
    }

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragMoveEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *event)
{
    if (m_ids.empty())
    {
        event->ignore();
        return false;
    }

    event->acceptProposedAction();
    return true;
}

bool DropInstrument::dragLeaveEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *)
{
    if (m_ids.empty())
        return false;

    return true;
}

bool DropInstrument::dropEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *event)
{
    QnWorkbenchContext *context = m_context.data();
    if (context == NULL)
        return true;

    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasFormat(Qn::NoSceneDrop))
        return false;

    const auto videoWallItems = resourcePool()->getVideoWallItemsByUuid(m_ids);

    // Try to drop videowall items first.
    if (delayedTriggerIfPossible(action::StartVideoWallControlAction, videoWallItems))
    {
        // Ignore resources.
    }
    else
    {
        const auto resources = resourcePool()->getResources(m_ids);
        if (!m_intoNewLayout)
        {
            delayedTriggerIfPossible(
                action::DropResourcesAction,
                action::Parameters(resources).withArgument(Qn::ItemPositionRole,
                    context->workbench()->mapper()->mapToGridF(event->scenePos()))
            );
        }
        else
        {
            delayedTriggerIfPossible(action::OpenInNewTabAction, resources);
        }
    }

    event->acceptProposedAction();
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

            QnRaiiGuard cursorGuard(
                []() { QApplication::setOverrideCursor(Qt::WaitCursor); },
                []() { QApplication::restoreOverrideCursor(); });

            context->menu()->triggerIfPossible(id, parameters);
        };

    executeDelayed(triggerIfPossible);
    return true;
}

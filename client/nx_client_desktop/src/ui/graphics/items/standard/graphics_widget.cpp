#include "graphics_widget.h"
#include "graphics_widget_p.h"

#include <cassert>

#include <QtCore/QVariant>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsLayout>
#include <QtWidgets/QStyleOptionTitleBar>
#include <QtWidgets/QApplication>

#include <utils/common/warnings.h>

#include <nx/client/core/utils/geometry.h>
#include <ui/common/frame_section.h>
#include <ui/common/constrained_geometrically.h>
#include <ui/common/scene_transformations.h>
#include <ui/common/cursor_cache.h>

#include "graphics_widget_scene_data.h"

using nx::client::core::Geometry;

namespace {
    const char *qn_sceneDataPropertyName = "_qn_sceneData";
    static qreal qn_graphicsWidget_defaultResizeEffectRadius = 8.0;
}


// -------------------------------------------------------------------------- //
// GraphicsWidgetPrivate
// -------------------------------------------------------------------------- //
GraphicsWidgetPrivate::GraphicsWidgetPrivate():
    q_ptr(NULL),
    handlingFlags(0),
    transformOrigin(GraphicsWidget::Legacy),
    resizeEffectRadius(qn_graphicsWidget_defaultResizeEffectRadius),
    inSetGeometry(false),
    windowData(NULL)
{
}

GraphicsWidgetPrivate::~GraphicsWidgetPrivate() {
    if(windowData)
        delete windowData;
}

GraphicsWidgetSceneData *GraphicsWidgetPrivate::ensureSceneData() {
    if(sceneData)
        return sceneData.data();

    QGraphicsScene *scene = q_func()->scene();
    if(!scene)
        return NULL;

    sceneData = scene->property(qn_sceneDataPropertyName).value<GraphicsWidgetSceneData *>();
    if(sceneData)
        return sceneData.data();

    sceneData = new GraphicsWidgetSceneData(scene, scene);
    scene->setProperty(qn_sceneDataPropertyName, QVariant::fromValue<GraphicsWidgetSceneData *>(sceneData.data()));

    return sceneData.data();
}

bool GraphicsWidgetPrivate::movableAncestorIsSelected(const QGraphicsItem *item) {
    const QGraphicsItem *parent = item->parentItem();
    return parent && (((parent->flags() & QGraphicsItem::ItemIsMovable) && parent->isSelected()) || movableAncestorIsSelected(parent));
}

bool GraphicsWidgetPrivate::hasDecoration() const {
    Q_Q(const GraphicsWidget);

    return (q->windowFlags() & Qt::Window) && (q->windowFlags() & Qt::WindowTitleHint);
}

void GraphicsWidgetPrivate::ensureWindowData() {
    if (!windowData)
        windowData = new WindowData;
}

void GraphicsWidgetPrivate::initStyleOptionTitleBar(QStyleOptionTitleBar *option) {
    Q_Q(GraphicsWidget);

    NX_ASSERT(option != NULL);

    ensureWindowData();

    q->initStyleOption(option);

    option->rect.setHeight(q->style()->pixelMetric(QStyle::PM_TitleBarHeight, option, NULL));
    option->titleBarFlags = q->windowFlags();
    option->subControls = QStyle::SC_TitleBarCloseButton | QStyle::SC_TitleBarLabel | QStyle::SC_TitleBarSysMenu;
    if(windowData->closeButtonHovered) {
        option->activeSubControls = QStyle::SC_TitleBarCloseButton;
    } else if(windowData->hoveredSection == Qt::TitleBarArea) {
        option->activeSubControls = QStyle::SC_TitleBarLabel;
    } else {
        option->activeSubControls = 0;
    }
    bool isActive = q->isActiveWindow();

    if (isActive) {
        option->state |= QStyle::State_Active;
        option->titleBarState = Qt::WindowActive;
        option->titleBarState |= QStyle::State_Active;
    } else {
        option->state &= ~QStyle::State_Active;
        option->titleBarState = Qt::WindowNoState;
    }
    QFont windowTitleFont = QApplication::font("QWorkspaceTitleBar");
    QRect textRect = q->style()->subControlRect(QStyle::CC_TitleBar, option, QStyle::SC_TitleBarLabel, NULL);
    option->text = QFontMetrics(windowTitleFont).elidedText(q->windowTitle(), Qt::ElideRight, textRect.width());
}

QRectF GraphicsWidgetPrivate::mapFromFrame(const QRectF &rect) {
    return rect.translated(q_func()->windowFrameRect().topLeft());
}

void GraphicsWidgetPrivate::mapToFrame(QStyleOptionTitleBar *option) {
    Q_Q(GraphicsWidget);

    NX_ASSERT(option != NULL);

    option->rect = q->windowFrameRect().toRect();
    option->rect.moveTo(0, 0);
    option->rect.setHeight(q->style()->pixelMetric(QStyle::PM_TitleBarHeight, option, NULL));
}

QPointF GraphicsWidgetPrivate::calculateTransformOrigin() const {
    Q_Q(const GraphicsWidget);

    QRectF rect = q->rect();
    switch(transformOrigin) {
    default:
        qnWarning("Invalid transform origin '%1'.", static_cast<int>(transformOrigin));
        /* Fall through. */
    case GraphicsWidget::Legacy:
        return q->transformOriginPoint();
    case GraphicsWidget::TopLeft:
        return QPointF(0, 0);
    case GraphicsWidget::Top:
        return QPointF(rect.width() / 2., 0);
    case GraphicsWidget::TopRight:
        return QPointF(rect.width(), 0);
    case GraphicsWidget::Left:
        return QPointF(0, rect.height() / 2.);
    case GraphicsWidget::Center:
        return QPointF(rect.width() / 2., rect.height() / 2.);
    case GraphicsWidget::Right:
        return QPointF(rect.width(), rect.height() / 2.);
    case GraphicsWidget::BottomLeft:
        return QPointF(0, rect.height());
    case GraphicsWidget::Bottom:
        return QPointF(rect.width() / 2., rect.height());
    case GraphicsWidget::BottomRight:
        return QPointF(rect.width(), rect.height());
    }
}



// -------------------------------------------------------------------------- //
// GraphicsWidget
// -------------------------------------------------------------------------- //
GraphicsWidget::GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    d_ptr(new GraphicsWidgetPrivate)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::~GraphicsWidget()
{
    /*
     * List of pending layout requests is stored in the scene data.
     * See QGraphicsWidgetPrivate::_q_relayout() for details.
     */
    if (GraphicsWidgetSceneData *sd = d_ptr->ensureSceneData())
        sd->pendingLayoutWidgets.remove(this);
}

void GraphicsWidget::initStyleOption(QStyleOption *option) const {
    base_type::initStyleOption(option);
}

GraphicsWidget::HandlingFlags GraphicsWidget::handlingFlags() const {
    return d_func()->handlingFlags;
}

void GraphicsWidget::setHandlingFlags(HandlingFlags handlingFlags) {
    Q_D(GraphicsWidget);

    if(d->handlingFlags == handlingFlags)
        return;
    handlingFlags = static_cast<HandlingFlags>(itemChange(ItemHandlingFlagsChange, static_cast<quint32>(handlingFlags)).toUInt());
    if(d->handlingFlags == handlingFlags)
        return;

    d->handlingFlags = handlingFlags;

    itemChange(ItemHandlingFlagsHaveChanged, static_cast<quint32>(handlingFlags));
}

void GraphicsWidget::setHandlingFlag(HandlingFlag flag, bool value) {
    setHandlingFlags(value ? d_func()->handlingFlags | flag : d_func()->handlingFlags & ~flag);
}

GraphicsWidget::TransformOrigin GraphicsWidget::transformOrigin() const {
    return d_func()->transformOrigin;
}

void GraphicsWidget::setTransformOrigin(TransformOrigin transformOrigin) {
    Q_D(GraphicsWidget);

    if (transformOrigin == d->transformOrigin)
        return;

    d->transformOrigin = transformOrigin;
    setTransformOriginPoint(d->calculateTransformOrigin());
    emit transformOriginChanged();
}

qreal GraphicsWidget::resizeEffectRadius() const {
    return d_func()->resizeEffectRadius;
}

void GraphicsWidget::setResizeEffectRadius(qreal resizeEffectRadius) {
    d_func()->resizeEffectRadius = resizeEffectRadius;
}

QMarginsF GraphicsWidget::contentsMargins() const
{
    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    return QMarginsF(left, top, right, bottom);
}

void GraphicsWidget::setContentsMargins(const QMarginsF& margins)
{
    setContentsMargins(margins.left(), margins.top(), margins.right(), margins.bottom());
}

QRectF GraphicsWidget::contentsRect() const {
    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    QRectF result = rect();
    result.setLeft(result.left() + left);
    result.setTop(result.top() + top);
    result.setRight(result.right() - right);
    result.setBottom(result.bottom() - bottom);
    return result;
}

void GraphicsWidget::handlePendingLayoutRequests(QGraphicsScene *scene) {
    if(!scene) {
        qnNullWarning(scene);
        return;
    }

    GraphicsWidgetSceneData *sd = scene->property(qn_sceneDataPropertyName).value<GraphicsWidgetSceneData *>();
    if(!sd)
        return;

    while(!sd->pendingLayoutWidgets.isEmpty()) {
        QSet<QGraphicsWidget *> widgets = sd->pendingLayoutWidgets;
        sd->pendingLayoutWidgets.clear();

        foreach(QGraphicsWidget *widget, widgets) {
            /* This code is copied from QGraphicsWidgetPrivate::_q_relayout(). */
            bool wasResized = widget->testAttribute(Qt::WA_Resized);
            widget->resize(widget->size());
            widget->setAttribute(Qt::WA_Resized, wasResized);
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void GraphicsWidget::setGeometry(const QRectF &rect) {
    Q_D(GraphicsWidget);

    if(d->inSetGeometry && rect == geometry())
        return; // TODO: #Elric #2395 patch QGraphicsWidget::setGeometry

    d->inSetGeometry = true;
    base_type::setGeometry(rect);
    d->inSetGeometry = false;

    if(d->transformOrigin != Legacy)
        setTransformOriginPoint(d->calculateTransformOrigin());
}

void GraphicsWidget::updateGeometry() {
    Q_D(GraphicsWidget);

    /* This code makes some adjustments to how QGraphicsWidget::updateGeometry works.
     * Check base implementation before trying to understand what is what. */

    if((d->handlingFlags & ItemHandlesLayoutRequests) || !QGraphicsLayout::instantInvalidatePropagation()) {
        base_type::updateGeometry();
        return;
    }

    if(!parentLayoutItem()) {
        if(GraphicsWidgetSceneData *sd = d->ensureSceneData()) {
            /* Skip QGraphicsWidget's implementation as it will post a relayout request
             * (actually a metacall to _q_relayout). */
            QGraphicsLayoutItem::updateGeometry();

            if(sd->pendingLayoutWidgets.empty())
                QApplication::postEvent(sd, new QEvent(GraphicsWidgetSceneData::HandlePendingLayoutRequests));

            sd->pendingLayoutWidgets.insert(this);
        } else {
            base_type::updateGeometry();
        }
    } else {
        base_type::updateGeometry();
    }
}

QVariant GraphicsWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    QVariant result = base_type::itemChange(change, value);

    if(change == ItemSceneChange) {
        if(GraphicsWidgetSceneData *sd = d_func()->ensureSceneData()) {
            sd->pendingLayoutWidgets.remove(this);
            d_func()->sceneData.clear();
        }
    }

    return result;
}

bool GraphicsWidget::event(QEvent *event) {
    /* Filter events that we want to handle by ourself. */
    switch(event->type()) {
    case QEvent::GraphicsSceneMousePress:
    case QEvent::GraphicsSceneMouseMove:
    case QEvent::GraphicsSceneMouseRelease:
    case QEvent::GraphicsSceneMouseDoubleClick:
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverMove:
    case QEvent::GraphicsSceneHoverLeave:
        break;
    default:
        return base_type::event(event);
    }

    /* Note that the following code is copied from QGraphicsWidget implementation,
     * so we don't need to call into the base class. */
    Q_D(GraphicsWidget);

    /* Forward the event to the layout first. */
    if (layout())
        layout()->widgetEvent(event);

    /* Handle the event itself. */
    switch (event->type()) {
    case QEvent::GraphicsSceneMousePress:
        if (windowFrameEvent(event))
            return true;
        break;
    case QEvent::GraphicsSceneMouseMove:
    case QEvent::GraphicsSceneMouseRelease:
    case QEvent::GraphicsSceneMouseDoubleClick:
        d->ensureWindowData();
        if (d->windowData->grabbedSection != Qt::NoSection)
            return windowFrameEvent(event);
        break;
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverMove:
    case QEvent::GraphicsSceneHoverLeave:
        windowFrameEvent(event);
        /* Filter out hover events if they were sent to us only because of the
         * decoration (special case in QGraphicsScenePrivate::dispatchHoverEvent). */
        if (!acceptHoverEvents())
            return true;
        break;
    default:
        break;
    }

    return QObject::event(event);
}

void GraphicsWidget::polishEvent() {
    base_type::polishEvent();

    if(GraphicsStyle *style = dynamic_cast<GraphicsStyle *>(this->style()))
        style->polish(this); // TODO: #Elric unpolish?
}

void GraphicsWidget::changeEvent(QEvent *event) {
    base_type::changeEvent(event);
}

void GraphicsWidget::selectThisWidget(bool clearOtherSelection)
{
    if (!isSelected() && (flags() & ItemIsSelectable))
    {
        if (clearOtherSelection)
        {
            QGraphicsScene *scene = this->scene();
            if (scene)
            {
                /* Don't emit multiple notifications. */
                bool signalsBlocked = scene->blockSignals(true);
                scene->clearSelection();
                scene->blockSignals(signalsBlocked);
            }
        }

        setSelected(true);
    }
}

void GraphicsWidget::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    /* The code is copied from QGraphicsItem implementation,
     * so we don't need to call into the base class. */

    if(event->button() == Qt::LeftButton && (flags() & ItemIsSelectable)) {
        bool multiSelect = (event->modifiers() & Qt::ControlModifier) != 0;
        if (!multiSelect)
            selectThisWidget();

    } else if(!(flags() & ItemIsMovable) || !(d_func()->handlingFlags & ItemHandlesMovement)) {
        event->ignore();
    }

    /* Qt::Popup closes when you click outside. */
    if((windowFlags() & Qt::Popup) == Qt::Popup) {
        event->accept();
        if(!rect().contains(event->pos()))
            close();
    }
}

void GraphicsWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    /* The code is copied from QGraphicsItem implementation,
     * so we don't need to call into the base class.
     *
     * Note that this code does not handle items that ignore transformations as
     * they are seriously broken anyway. */
    Q_D(GraphicsWidget);

    if((event->buttons() & Qt::LeftButton) && (flags() & ItemIsMovable) && (d->handlingFlags & ItemHandlesMovement)) {
        GraphicsWidgetSceneData *sd = d->ensureSceneData();

        /* Determine the list of items that need to be moved. */
        QList<QGraphicsItem *> selectedItems;
        QHash<QGraphicsItem *, QPointF> initialPositions;
        if (scene() && sd) {
            if(flags() & ItemIsSelectable)
                selectedItems = scene()->selectedItems(); /* Drag selected items only if current item is selectable. */
            initialPositions = sd->movingItemsInitialPositions;
            if (initialPositions.isEmpty()) {
                foreach (QGraphicsItem *item, selectedItems)
                    initialPositions[item] = item->pos();
                initialPositions[this] = pos();
                sd->movingItemsInitialPositions = initialPositions;
            }
        }

        /* Move all selected items. */
        int i = 0;
        bool movedMe = false;
        while(i <= selectedItems.size()) {
            QGraphicsItem *item = NULL;
            if(i < selectedItems.size()) {
                item = selectedItems[i];
            } else {
                item = this;
            }
            if(item == this) {
                /* Slightly clumsy-looking way to ensure that "this" is part
                 * of the list of items to move, this is to avoid allocations
                 * (appending this item to the list of selected items causes a
                 * detach). */
                if(movedMe)
                    break;
                movedMe = true;
            }

#ifdef _DEBUG
            if(item->flags() & ItemIgnoresTransformations)
                qnWarning("Proper dragging of items that ignore transformations is not supported.");
#endif

            if((item->flags() & ItemIsMovable) && !d->movableAncestorIsSelected()) {
                QPointF currentParentPos = item->mapToParent(item->mapFromScene(event->scenePos()));
                QPointF buttonDownParentPos = item->mapToParent(item->mapFromScene(event->buttonDownScenePos(Qt::LeftButton)));

                item->setPos(initialPositions.value(item) + currentParentPos - buttonDownParentPos);

                if (item->flags() & ItemIsSelectable)
                    item->setSelected(true);
            }

            ++i;
        }
    } else {
        event->ignore();
    }
}

void GraphicsWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    /* The code is copied from QGraphicsItem implementation,
     * so we don't need to call into the base class. */
    Q_D(GraphicsWidget);

    if(flags() & ItemIsSelectable) {
        bool multiSelect = (event->modifiers() & Qt::ControlModifier) != 0;
        if(event->scenePos() == event->buttonDownScenePos(Qt::LeftButton)) {
            /* The item didn't move. */
            if(multiSelect) {
                setSelected(!isSelected());
            } else {
                if (QGraphicsScene *scene = this->scene()) {
                    /* Don't emit multiple notifications. */
                    bool signalsBlocked = scene->blockSignals(true);
                    scene->clearSelection();
                    scene->blockSignals(signalsBlocked);
                }
                setSelected(true);
            }
        }
    }

    if(!event->buttons() && (d->handlingFlags & ItemHandlesMovement))
        if(GraphicsWidgetSceneData *sd = d->ensureSceneData())
            sd->movingItemsInitialPositions.clear();
}

Qt::WindowFrameSection GraphicsWidget::windowFrameSectionAt(const QPointF& pos) const {
    return windowFrameSectionAt(QRectF(pos, QSizeF(0, 0)));
}

Qn::WindowFrameSections GraphicsWidget::windowFrameSectionsAt(const QRectF &region) const {
    Q_D(const GraphicsWidget);

    if(!d->hasDecoration()) {
        return Qn::NoSection;
    } else {
        return Qn::calculateRectangularFrameSections(windowFrameRect(), rect(), region);
    }
}

Qn::WindowFrameSections GraphicsWidgetPrivate::resizingFrameSectionsAt(const QPointF &pos, QWidget *viewport) const {
    Q_Q(const GraphicsWidget);

    QGraphicsView *view = viewport ? dynamic_cast<QGraphicsView *>(viewport->parentWidget()) : NULL;
    if(!view)
        return q->windowFrameSectionsAt(QRectF(pos, QSizeF(0.0, 0.0)));

    QRectF effectRect = q->mapRectFromScene(QnSceneTransformations::mapRectToScene(view, QRectF(0, 0, resizeEffectRadius, resizeEffectRadius)));
    qreal effectRadius = qMax(effectRect.width(), effectRect.height());
    return q->windowFrameSectionsAt(QRectF(pos - QPointF(effectRadius, effectRadius), QSizeF(2 * effectRadius, 2 * effectRadius)));
}

Qt::WindowFrameSection GraphicsWidgetPrivate::resizingFrameSectionAt(const QPointF &pos, QWidget *viewport) const {
    return Qn::toNaturalQtFrameSection(resizingFrameSectionsAt(pos, viewport));
}

bool GraphicsWidget::windowFrameEvent(QEvent *event) {
    /* The code is copied from QGraphicsWidget implementation,
     * so we don't need to call into the base class. */
    Q_D(GraphicsWidget);

    switch (event->type()) {
    case QEvent::GraphicsSceneMousePress:
        return d->windowFrameMousePressEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseMove:
        return d->windowFrameMouseMoveEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseRelease:
        return d->windowFrameMouseReleaseEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneHoverMove:
        return d->windowFrameHoverMoveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
    case QEvent::GraphicsSceneHoverLeave:
        return d->windowFrameHoverLeaveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
    default:
        return false;
    }
}

bool GraphicsWidgetPrivate::windowFrameHoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    Q_Q(GraphicsWidget);

    if (!hasDecoration()) // TODO: #Elric invalid check
        return false;

    ensureWindowData();
    bool oldCloseButtonHovered = windowData->closeButtonHovered;
    windowData->closeButtonHovered = false;

    Qt::WindowFrameSection section = resizingFrameSectionAt(event->pos(), event->widget());
    if(section == Qt::TitleBarArea) {
        /* Make sure that the coordinates (rect and pos) we send to the style are positive. */
        QStyleOptionTitleBar option;
        initStyleOptionTitleBar(&option);
        mapToFrame(&option);

        windowData->closeButtonRect = mapFromFrame(q->style()->subControlRect(QStyle::CC_TitleBar, &option, QStyle::SC_TitleBarCloseButton, NULL));
        windowData->closeButtonHovered = windowData->closeButtonRect.contains(event->pos());
    }
    windowData->hoveredSection = section;

    /* Update cursor. */
    if(handlingFlags & GraphicsWidget::ItemHandlesResizing) {
        QCursor cursor = q->windowCursorAt(section);
        if(q->cursor().shape() != cursor.shape() || cursor.shape() == Qt::BitmapCursor)
            q->setCursor(cursor);
    }

    if (windowData->closeButtonHovered != oldCloseButtonHovered)
        q->update(windowData->closeButtonRect);

    return section != Qt::NoSection;
}

bool GraphicsWidgetPrivate::windowFrameHoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    Q_UNUSED(event)
    Q_Q(GraphicsWidget);

    if (hasDecoration()) { // TODO: #Elric invalid check
        /* Reset cursor. */
        if(handlingFlags & GraphicsWidget::ItemHandlesResizing)
            q->unsetCursor();

        ensureWindowData();

        if (windowData->closeButtonHovered)
            q->update(windowData->closeButtonRect);

        /* Update the hover state. */
        windowData->hoveredSection = Qt::NoSection;
        windowData->closeButtonHovered = false;
        windowData->closeButtonRect = QRectF();
    }

    return true;
}

bool GraphicsWidgetPrivate::windowFrameMousePressEvent(QGraphicsSceneMouseEvent *event) {
    Q_Q(GraphicsWidget);

    if (event->button() != Qt::LeftButton)
        return false;

    ensureWindowData();
    windowData->grabbedSection = resizingFrameSectionAt(event->pos(), event->widget());
    windowData->startSize = q->size();
    windowData->constrained = dynamic_cast<ConstrainedGeometrically *>(q); /* We do dynamic_cast every time to feel safer during destruction. */

    if (windowData->closeButtonHovered) {
        windowData->closeButtonGrabbed = true;
        windowData->grabbedSection = Qt::NoSection; /* User cannot grab the title bar by pressing close button. */
        q->update();
    }

    switch(windowData->grabbedSection) {
    case Qt::TitleBarArea:
        if(handlingFlags & GraphicsWidget::ItemHandlesMovement) {
            QSizeF size = q->size();
            windowData->startPinPoint = Geometry::cwiseDiv(event->pos(), size);

            /* Make sure we don't get NaNs in startPinPoint. */
            if(qFuzzyIsNull(size.width()))
                windowData->startPinPoint.setX(0.0);
            if(qFuzzyIsNull(size.height()))
                windowData->startPinPoint.setY(0.0);

            return true;
        } else {
            windowData->grabbedSection = Qt::NoSection;
            return false;
        }
    case Qt::LeftSection:
    case Qt::TopLeftSection:
    case Qt::TopSection:
    case Qt::TopRightSection:
    case Qt::RightSection:
    case Qt::BottomRightSection:
    case Qt::BottomSection:
    case Qt::BottomLeftSection:
        if(handlingFlags & GraphicsWidget::ItemHandlesResizing) {
            windowData->startPinPoint = q->mapToParent(Qn::calculatePinPoint(q->rect(), windowData->grabbedSection));
            return true;
        } else {
            windowData->grabbedSection = Qt::NoSection;
            return false;
        }
    case Qt::NoSection:
        //event->setAccepted(windowData->closeButtonGrabbed); // TODO: #Elric
        return false;
    default:
        return false;
    }
}

bool GraphicsWidgetPrivate::windowFrameMouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    Q_Q(GraphicsWidget);

    ensureWindowData();
    if(windowData->closeButtonGrabbed) {
        bool oldCloseButtonHovered = windowData->closeButtonHovered;
        windowData->closeButtonHovered = windowData->closeButtonRect.contains(event->pos());
        if(oldCloseButtonHovered != windowData->closeButtonHovered)
            q->update(windowData->closeButtonRect);
        return true;
    }

    if(!(event->buttons() & Qt::LeftButton))
        return false;

    if(windowData->grabbedSection != Qt::NoSection) {
        QSizeF newSize;
        if(windowData->grabbedSection == Qt::TitleBarArea) {
            newSize = q->size();
        } else {
            newSize = windowData->startSize + Qn::calculateSizeDelta(
                event->pos() - q->mapFromScene(event->buttonDownScenePos(Qt::LeftButton)),
                windowData->grabbedSection
            );
            QSizeF minSize = q->effectiveSizeHint(Qt::MinimumSize);
            QSizeF maxSize = q->effectiveSizeHint(Qt::MaximumSize);
            newSize = QSizeF(
                qBound(minSize.width(), newSize.width(), maxSize.width()),
                qBound(minSize.height(), newSize.height(), maxSize.height())
            );
            /* We don't handle heightForWidth. */
        }

        QPointF newPos;
        if(windowData->grabbedSection == Qt::TitleBarArea) {
            newPos = q->mapToParent(event->pos() - Geometry::cwiseMul(windowData->startPinPoint, newSize));
        } else {
            newPos = q->pos() + windowData->startPinPoint - q->mapToParent(Qn::calculatePinPoint(QRectF(QPointF(0.0, 0.0), newSize), windowData->grabbedSection));
        }

        if(windowData->constrained != NULL) {
            QRectF newGeometry = windowData->constrained->constrainedGeometry(QRectF(newPos, newSize), windowData->grabbedSection);
            newSize = newGeometry.size();
            newPos = newGeometry.topLeft();
        }

        /* Change size & position. */
        q->resize(newSize);
        q->setPos(newPos);

        return true;
    } else {
        return false;
    }
}

bool GraphicsWidgetPrivate::windowFrameMouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    Q_Q(GraphicsWidget);

    ensureWindowData();
    if (windowData->closeButtonGrabbed) {
        windowData->closeButtonGrabbed = false;

        if(windowData->closeButtonHovered)
            q->close();

        return true;
    } else if(windowData->grabbedSection != Qt::NoSection) {
        if(event->buttons() == 0)
            windowData->grabbedSection = Qt::NoSection;

        return true;
    } else {
        return false;
    }
}




#include "graphics_widget.h"
#include "graphics_widget_p.h"

#include <cassert>

#include <QtCore/QVariant>
#include <QtGui/QWidget>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsLayout>
#include <QtGui/QStyleOptionTitleBar>
#include <QtGui/QApplication>

#include <utils/common/warnings.h>

#include <ui/common/frame_section.h>

class GraphicsWidgetSceneData: public QObject {
public:
    /** Event type for scene-wide layout requests. */
    static const QEvent::Type HandlePendingLayoutRequests = static_cast<QEvent::Type>(QEvent::User + 0x19FA);

    GraphicsWidgetSceneData(QGraphicsScene *scene, QObject *parent = NULL): 
        QObject(parent), 
        scene(scene) 
    {
        assert(scene);
    }

    virtual bool event(QEvent *event) override {
        if(event->type() == HandlePendingLayoutRequests) {
            GraphicsWidget::handlePendingLayoutRequests(scene);
            return true;
        } else {
            return QObject::event(event);
        }
    }

    QGraphicsScene *scene;
    QHash<QGraphicsItem *, QPointF> movingItemsInitialPositions;
    QSet<QGraphicsWidget *> pendingLayoutWidgets;
};

Q_DECLARE_METATYPE(GraphicsWidgetSceneData *);

namespace {
    const char *qn_sceneDataPropertyName = "_qn_sceneData";
}


// -------------------------------------------------------------------------- //
// GraphicsWidgetPrivate
// -------------------------------------------------------------------------- //
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

    sceneData = new GraphicsWidgetSceneData(scene);
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

    assert(option != NULL);

    ensureWindowData();

    q->initStyleOption(option);

    option->rect.setHeight(q->style()->pixelMetric(QStyle::PM_TitleBarHeight, option, q));
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
    QRect textRect = q->style()->subControlRect(QStyle::CC_TitleBar, option, QStyle::SC_TitleBarLabel, q);
    option->text = QFontMetrics(windowTitleFont).elidedText(q->windowTitle(), Qt::ElideRight, textRect.width());
}

QRectF GraphicsWidgetPrivate::mapFromFrame(const QRectF &rect) {
    return rect.translated(q_func()->windowFrameRect().topLeft());
}

void GraphicsWidgetPrivate::mapToFrame(QStyleOptionTitleBar *option) {
    Q_Q(GraphicsWidget);

    assert(option != NULL);

    option->rect = q->windowFrameRect().toRect();
    option->rect.moveTo(0, 0);
    option->rect.setHeight(q->style()->pixelMetric(QStyle::PM_TitleBarHeight, option, q));
}


// -------------------------------------------------------------------------- //
// GraphicsWidget
// -------------------------------------------------------------------------- //
GraphicsWidget::GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsWidget(parent, windowFlags),
    d_ptr(new GraphicsWidgetPrivate)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsWidget(parent, windowFlags),
    d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::~GraphicsWidget() {
    if(GraphicsWidgetSceneData *sd = d_func()->ensureSceneData())
        sd->pendingLayoutWidgets.remove(this);
}

GraphicsStyle *GraphicsWidget::style() const {
    Q_D(const GraphicsWidget);

    if(!d->style) {
        d->style = dynamic_cast<GraphicsStyle *>(base_type::style());

        if(!d->style) {
            if(!d->reserveStyle)
                d->reserveStyle.reset(new GraphicsStyle());

            d->reserveStyle->setBaseStyle(base_type::style());
            d->style = d->reserveStyle.data();
        }
    }

    return d->style;
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

void GraphicsWidget::setStyle(GraphicsStyle *style) {
    setStyle(style->baseStyle());
}

QRectF GraphicsWidget::contentsRect() const {
    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    QRectF result = rect();
    result.setLeft(result.left() + left);
    result.setTop(result.top() + top);
    result.setRect(result.right() - right);
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

    if(change == ItemSceneHasChanged)
        d_func()->sceneData.clear();

    return result;
}

void GraphicsWidget::changeEvent(QEvent *event) {
    Q_D(GraphicsWidget);

    base_type::changeEvent(event);

    if(event->type() == QEvent::StyleChange)
        d->style = NULL;
}

void GraphicsWidget::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    /* The code is copied from QGraphicsItem implementation, 
     * so we don't need to call into the base class. */

    if(event->button() == Qt::LeftButton && (flags() & ItemIsSelectable)) {
        bool multiSelect = (event->modifiers() & Qt::ControlModifier) != 0;
        if (!multiSelect) {
            if (!isSelected() && (flags() & ItemIsSelectable)) {
                if (QGraphicsScene *scene = this->scene()) {
                    /* Don't emit multiple notifications. */
                    bool signalsBlocked = scene->blockSignals(true);
                    scene->clearSelection();
                    scene->blockSignals(signalsBlocked);
                } 
                    
                setSelected(true);
            }
        }
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
    return base_type::windowFrameSectionAt(pos);
}

bool GraphicsWidget::windowFrameEvent(QEvent *event) {
    /* The code is copied from QGraphicsWidget implementation, 
     * so we don't need to call into the base class. */
    Q_D(GraphicsWidget);

    switch (event->type()) {
    case QEvent::GraphicsSceneMousePress:
        d->windowFrameMousePressEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        break;
    case QEvent::GraphicsSceneMouseMove:
        d->windowFrameMouseMoveEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        break;
    case QEvent::GraphicsSceneMouseRelease:
        d->windowFrameMouseReleaseEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        break;
    case QEvent::GraphicsSceneHoverMove:
        d->windowFrameHoverMoveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        break;
    case QEvent::GraphicsSceneHoverLeave:
        d->windowFrameHoverLeaveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        break;
    default:
        break;
    }
    
    return event->isAccepted();
}

void GraphicsWidgetPrivate::windowFrameHoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    Q_Q(GraphicsWidget);

    if (!hasDecoration())
        return;

    if (q->rect().contains(event->pos())) {
        /* Mouse has left the window frame and entered its interior. */
        ensureWindowData();
        if (windowData->hoveredSection != Qt::NoSection)
            windowFrameHoverLeaveEvent(event);
        return;
    }

    ensureWindowData();
    bool oldCloseButtonHovered = windowData->closeButtonHovered;
    windowData->closeButtonHovered = false;
    
    /* Make sure that the coordinates (rect and pos) we send to the style are positive. */
    QStyleOptionTitleBar option;
    initStyleOptionTitleBar(&option);
    mapToFrame(&option);

    Qt::WindowFrameSection section = q->windowFrameSectionAt(event->pos());
    if(section == Qt::TitleBarArea) {
        windowData->closeButtonRect = mapFromFrame(q->style()->subControlRect(QStyle::CC_TitleBar, &option, QStyle::SC_TitleBarCloseButton, q));
        windowData->closeButtonHovered = windowData->closeButtonRect.contains(event->pos());
    } else if(section == Qt::NoSection) {
        event->ignore();
    }

    /* Update cursor. */
    if(handlingFlags & GraphicsWidget::ItemHandlesResizing) {
        Qt::CursorShape cursorShape = Qn::calculateHoverCursorShape(section);
        if(q->cursor().shape() != cursorShape)
            q->setCursor(cursorShape);
    }

    if (windowData->closeButtonHovered != oldCloseButtonHovered)
        q->update(windowData->closeButtonRect);
}

void GraphicsWidgetPrivate::windowFrameHoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    Q_Q(GraphicsWidget);

    Q_UNUSED(event);

    if (hasDecoration()) {
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
}

void GraphicsWidgetPrivate::windowFrameMousePressEvent(QGraphicsSceneMouseEvent *event) {
    Q_Q(GraphicsWidget);
    if (event->button() != Qt::LeftButton)
        return;

    ensureWindowData();
    windowData->grabbedSection = q->windowFrameSectionAt(event->pos());
    windowData->startSize = q->size();

    if (windowData->closeButtonHovered) {
        windowData->closeButtonGrabbed = true;
        windowData->grabbedSection = Qt::NoSection; /* User cannot grab the title bar by pressing close button. */
        q->update();
    }

    switch(windowData->grabbedSection) {
    case Qt::TitleBarArea:
        if(handlingFlags & GraphicsWidget::ItemHandlesMovement) {
            event->accept();
        } else {
            windowData->grabbedSection = Qt::NoSection;
            event->ignore();
        }
        break;
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
            event->accept();
        } else {
            windowData->grabbedSection = Qt::NoSection;
            event->ignore();
        }
        break;
    case Qt::NoSection:
        event->setAccepted(windowData->closeButtonGrabbed);
        break;
    default:
        event->ignore();
        break;
    }
}

void GraphicsWidgetPrivate::windowFrameMouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    Q_Q(GraphicsWidget);

    ensureWindowData();
    if(windowData->closeButtonGrabbed) {
        bool oldCloseButtonHovered = windowData->closeButtonHovered;
        windowData->closeButtonHovered = windowData->closeButtonRect.contains(event->pos());
        if(oldCloseButtonHovered != windowData->closeButtonHovered)
            q->update(windowData->closeButtonRect);
        event->accept();
        return;
    }

    if (!(event->buttons() & Qt::LeftButton)) {
        event->ignore();
        return;
    }

    if(windowData->grabbedSection != Qt::NoSection) {
        QSizeF newSize = windowData->startSize + Qn::calculateSizeDelta(
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

        /* Change size & position. */
        q->resize(newSize);
        if(windowData->grabbedSection != Qt::TitleBarArea)
            q->setPos(q->pos() + windowData->startPinPoint - q->mapToParent(Qn::calculatePinPoint(q->rect(), windowData->grabbedSection)));
        
        event->accept();
    }
}

void GraphicsWidgetPrivate::windowFrameMouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    Q_Q(GraphicsWidget);

    ensureWindowData();
    if (windowData->closeButtonGrabbed) {
        windowData->closeButtonGrabbed = false;

        if(windowData->closeButtonHovered)
            q->close();

        event->accept();
    } else if(windowData->grabbedSection != Qt::NoSection) {
        if(event->buttons() == 0)
            windowData->grabbedSection = Qt::NoSection;

        event->accept();
    }
}




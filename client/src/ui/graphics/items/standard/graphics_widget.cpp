#include "graphics_widget.h"
#include "graphics_widget_p.h"

#include <QtCore/QVariant>
#include <QtGui/QWidget>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsScene>

#include <utils/common/warnings.h>

class GraphicsWidgetSceneData: public QObject {
public:
    GraphicsWidgetSceneData(QObject *parent = NULL): QObject(parent) {}

    QHash<QGraphicsItem *, QPointF> movingItemsInitialPositions;
};

Q_DECLARE_METATYPE(GraphicsWidgetSceneData *);

namespace {
    const char *qn_sceneDataPropertyName = "_qn_sceneData";
}


// -------------------------------------------------------------------------- //
// GraphicsWidgetPrivate
// -------------------------------------------------------------------------- //
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
    return;
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
    if (d->handlingFlags == handlingFlags)
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


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
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

    if (event->button() == Qt::LeftButton && (flags() & ItemIsSelectable)) {
        bool multiSelect = (event->modifiers() & Qt::ControlModifier) != 0;
        if (!multiSelect) {
            if (!isSelected()) {
                if (QGraphicsScene *scene = this->scene()) {
                    /* Don't emit multiple notifications. */
                    bool signalsBlocked = scene->blockSignals(true);
                    scene->clearSelection();
                    scene->blockSignals(signalsBlocked);
                } 
                    
                setSelected(true);
            }
        }
    } else if (!(flags() & ItemIsMovable) || !(d_func()->handlingFlags & ItemHandlesMovement)) {
        event->ignore();
    }

    /* Qt::Popup closes when you click outside. */
    if ((windowFlags() & Qt::Popup) == Qt::Popup) {
        event->accept();
        if (!rect().contains(event->pos()))
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

    if ((event->buttons() & Qt::LeftButton) && (flags() & ItemIsMovable) && (d->handlingFlags & ItemHandlesMovement)) {
        GraphicsWidgetSceneData *sd = d->ensureSceneData();

        /* Determine the list of items that need to be moved. */
        QList<QGraphicsItem *> selectedItems;
        QHash<QGraphicsItem *, QPointF> initialPositions;
        if (scene() && sd) {
            selectedItems = scene()->selectedItems();
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
        while (i <= selectedItems.size()) {
            QGraphicsItem *item = NULL;
            if (i < selectedItems.size()) {
                item = selectedItems[i];
            } else {
                item = this;
            }
            if (item == this) {
                /* Slightly clumsy-looking way to ensure that "this" is part
                 * of the list of items to move, this is to avoid allocations
                 * (appending this item to the list of selected items causes a
                 * detach). */
                if (movedMe)
                    break;
                movedMe = true;
            }

#ifdef _DEBUG
            if (item->flags() & ItemIgnoresTransformations)
                qnWarning("Proper dragging of items that ignore transformations is not supported.");
#endif

            if ((item->flags() & ItemIsMovable) && !GraphicsWidgetPrivate::movableAncestorIsSelected(item)) {
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

    if (flags() & ItemIsSelectable) {
        bool multiSelect = (event->modifiers() & Qt::ControlModifier) != 0;
        if (event->scenePos() == event->buttonDownScenePos(Qt::LeftButton)) {
            /* The item didn't move. */
            if (multiSelect) {
                setSelected(!isSelected());
            } else {
                bool selectionChanged = false;
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
    
    if (!event->buttons() && (d->handlingFlags & ItemHandlesMovement))
        if(GraphicsWidgetSceneData *sd = d->ensureSceneData())
            sd->movingItemsInitialPositions.clear();
}




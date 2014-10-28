#include "ptz_instrument.h"
#include "ptz_instrument_p.h"

#include <cassert>

#include <QtCore/QVariant>
#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>
#include <utils/math/math.h>
#include <utils/math/color_transformations.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/animation/opacity_animator.h>
#include <ui/animation/animation_event.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>

//#define QN_PTZ_INSTRUMENT_DEBUG
#ifdef QN_PTZ_INSTRUMENT_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

namespace {
    const qreal instantSpeedUpdateThreshold = 0.1;
    const qreal speedUpdateThreshold = 0.001;
    const int speedUpdateIntervalMSec = 500;

    const double minPtzZoomRectSize = 0.08;

    const qreal itemUnzoomThreshold = 0.975; /* In sync with hardcoded constant in workbench_controller */ // TODO: #Elric

    Qt::Orientations capabilitiesToMode(Qn::PtzCapabilities capabilities) {
        Qt::Orientations result = 0;
        bool isFisheye = ((capabilities & Qn::VirtualPtzCapability) == Qn::VirtualPtzCapability);
        if (isFisheye)
            return result;

        if ((capabilities & Qn::ContinuousPanCapability) == Qn::ContinuousPanCapability)
            result |= Qt::Horizontal;

        if ((capabilities & Qn::ContinuousTiltCapability) == Qn::ContinuousTiltCapability)
            result |= Qt::Vertical;
        return result;
    }
}


// -------------------------------------------------------------------------- //
// PtzInstrument
// -------------------------------------------------------------------------- //
PtzInstrument::PtzInstrument(QObject *parent): 
    base_type(
        makeSet(QEvent::MouseButtonPress, AnimationEvent::Animation),
        makeSet(),
        makeSet(),
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), 
        parent
    ),
    QnWorkbenchContextAware(parent),
    m_clickDelayMSec(QApplication::doubleClickInterval()),
    m_expansionSpeed(qnGlobals->workbenchUnitSize() / 5.0),
    m_movement(NoMovement)
{}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();
}

QnMediaResourceWidget *PtzInstrument::target() const {
    return m_target.data();
}

PtzManipulatorWidget *PtzInstrument::targetManipulator() const {
    return overlayWidget(target())->manipulatorWidget();
}

QnSplashItem *PtzInstrument::newSplashItem(QGraphicsItem *parentItem) {
    QnSplashItem *result;
    if(!m_freeAnimations.empty()) {
        result = m_freeAnimations.back().item;
        m_freeAnimations.pop_back();
        result->setParentItem(parentItem);
    } else {
        result = new PtzSplashItem(parentItem);
        connect(result, &QObject::destroyed, this, &PtzInstrument::at_splashItem_destroyed);
    }

    result->setOpacity(0.0);
    result->show();
    return result;
}

PtzOverlayWidget *PtzInstrument::overlayWidget(QnMediaResourceWidget *widget) const {
    return m_dataByWidget[widget].overlayWidget;
}

PtzOverlayWidget *PtzInstrument::ensureOverlayWidget(QnMediaResourceWidget *widget) {
    PtzData &data = m_dataByWidget[widget];

    if(data.overlayWidget)
        return data.overlayWidget;

    bool isFisheye = data.hasCapabilities(Qn::VirtualPtzCapability);
    bool isFisheyeEnabled = widget->dewarpingParams().enabled;

    PtzOverlayWidget *overlay = new PtzOverlayWidget();
    overlay->setOpacity(0.0);
    overlay->manipulatorWidget()->setCursor(Qt::SizeAllCursor);
    overlay->zoomInButton()->setTarget(widget);
    overlay->zoomOutButton()->setTarget(widget);
    overlay->focusInButton()->setTarget(widget);
    overlay->focusOutButton()->setTarget(widget);
    overlay->focusAutoButton()->setTarget(widget);
    overlay->modeButton()->setTarget(widget);
    overlay->modeButton()->setVisible(isFisheye && isFisheyeEnabled);
    overlay->setMarkersMode(capabilitiesToMode(data.capabilities));

    data.overlayWidget = overlay;

    connect(overlay->zoomInButton(),    &QnImageButtonWidget::pressed,  this, &PtzInstrument::at_zoomInButton_pressed);
    connect(overlay->zoomInButton(),    &QnImageButtonWidget::released, this, &PtzInstrument::at_zoomInButton_released);
    connect(overlay->zoomOutButton(),   &QnImageButtonWidget::pressed,  this, &PtzInstrument::at_zoomOutButton_pressed);
    connect(overlay->zoomOutButton(),   &QnImageButtonWidget::released, this, &PtzInstrument::at_zoomOutButton_released);
    connect(overlay->focusInButton(),   &QnImageButtonWidget::pressed,  this, &PtzInstrument::at_focusInButton_pressed);
    connect(overlay->focusInButton(),   &QnImageButtonWidget::released, this, &PtzInstrument::at_focusInButton_released);
    connect(overlay->focusOutButton(),  &QnImageButtonWidget::pressed,  this, &PtzInstrument::at_focusOutButton_pressed);
    connect(overlay->focusOutButton(),  &QnImageButtonWidget::released, this, &PtzInstrument::at_focusOutButton_released);
    connect(overlay->focusAutoButton(), &QnImageButtonWidget::clicked,  this, &PtzInstrument::at_focusAutoButton_clicked);
    connect(overlay->modeButton(),      &QnImageButtonWidget::clicked,  this, &PtzInstrument::at_modeButton_clicked);

    widget->addOverlayWidget(overlay, QnResourceWidget::Invisible, true, false, false);

    return overlay;
}

FixedArSelectionItem *PtzInstrument::selectionItem() const {
    return m_selectionItem.data();
}

void PtzInstrument::ensureSelectionItem() {
    if(selectionItem())
        return;

    m_selectionItem = new PtzSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setElementSize(qnGlobals->workbenchUnitSize() / 64.0);
    selectionItem()->setOptions(FixedArSelectionItem::DrawCentralElement | FixedArSelectionItem::DrawSideElements);

    if(scene())
        scene()->addItem(selectionItem());
}

PtzElementsWidget *PtzInstrument::elementsWidget() const {
    return m_elementsWidget.data();
}

void PtzInstrument::ensureElementsWidget() {
    if(elementsWidget())
        return;

    m_elementsWidget = new PtzElementsWidget();
    display()->setLayer(elementsWidget(), Qn::EffectsLayer);
    
    if(scene())
        scene()->addItem(elementsWidget());
}

void PtzInstrument::updateOverlayWidget() {
    updateOverlayWidgetInternal(checked_cast<QnMediaResourceWidget *>(sender()));
}

void PtzInstrument::updateOverlayWidgetInternal(QnMediaResourceWidget *widget) {
    bool hasCrosshair = widget->options() & QnResourceWidget::DisplayCrosshair;

    if(hasCrosshair)
        ensureOverlayWidget(widget);

    QnResourceWidget::OverlayVisibility visibility = hasCrosshair ? QnResourceWidget::AutoVisible : QnResourceWidget::Invisible;

    if(PtzOverlayWidget *overlayWidget = this->overlayWidget(widget)) {
        widget->setOverlayWidgetVisibility(overlayWidget, visibility);

        const PtzData &data = m_dataByWidget[widget];

        bool isFisheye = data.hasCapabilities(Qn::VirtualPtzCapability);
        bool isFisheyeEnabled = widget->dewarpingParams().enabled;

        overlayWidget->manipulatorWidget()->setVisible(data.hasCapabilities(Qn::ContinuousPanCapability) || data.hasCapabilities(Qn::ContinuousTiltCapability));
        overlayWidget->zoomInButton()->setVisible(data.hasCapabilities(Qn::ContinuousZoomCapability));
        overlayWidget->zoomOutButton()->setVisible(data.hasCapabilities(Qn::ContinuousZoomCapability));
        overlayWidget->focusInButton()->setVisible(data.hasCapabilities(Qn::ContinuousFocusCapability));
        overlayWidget->focusOutButton()->setVisible(data.hasCapabilities(Qn::ContinuousFocusCapability));
        overlayWidget->focusAutoButton()->setVisible(data.traits.contains(Qn::ManualAutoFocusPtzTrait));
        
        if (isFisheye) {
            int panoAngle = widget->item()
                    ? 90 * widget->item()->dewarpingParams().panoFactor
                    : 90;
            overlayWidget->modeButton()->setText(QString::number(panoAngle));
        }
        overlayWidget->modeButton()->setVisible(isFisheye && isFisheyeEnabled);
        overlayWidget->setMarkersMode(capabilitiesToMode(data.capabilities));
    }
}

void PtzInstrument::updateCapabilities(QnMediaResourceWidget *widget) {
    PtzData &data = m_dataByWidget[widget];
    
    Qn::PtzCapabilities capabilities = widget->ptzController()->getCapabilities();
    if(data.capabilities == capabilities)
        return;

    if((data.capabilities ^ capabilities) & Qn::AuxilaryPtzCapability)
        updateTraits(widget);

    data.capabilities = capabilities;
    updateOverlayWidgetInternal(widget);
}

void PtzInstrument::updateTraits(QnMediaResourceWidget *widget) {
    PtzData &data = m_dataByWidget[widget];

    QnPtzAuxilaryTraitList traits;
    widget->ptzController()->getAuxilaryTraits(&traits);
    if(data.traits == traits)
        return;

    data.traits = traits;
    updateOverlayWidgetInternal(widget);
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos) {
    ptzMoveTo(widget, QRectF(pos - toPoint(widget->size() / 2), widget->size()));
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect) {
    qreal aspectRatio = QnGeometry::aspectRatio(widget->size());
    QRectF viewport = QnGeometry::cwiseDiv(rect, widget->size());
    widget->ptzController()->viewportMove(aspectRatio, viewport, 1.0);
}

void PtzInstrument::ptzUnzoom(QnMediaResourceWidget *widget) {
    QSizeF size = widget->size() * 100;
    ptzMoveTo(widget, QRectF(widget->rect().center() - toPoint(size) / 2, size));
}

void PtzInstrument::ptzMove(QnMediaResourceWidget *widget, const QVector3D &speed, bool instant) {
    PtzData &data = m_dataByWidget[widget];
    data.requestedSpeed = speed;

    if(!instant && (data.currentSpeed - data.requestedSpeed).lengthSquared() < speedUpdateThreshold * speedUpdateThreshold)
        return;

    instant = instant ||
        (qFuzzyIsNull(data.currentSpeed) ^ qFuzzyIsNull(data.requestedSpeed)) || 
        (data.currentSpeed - data.requestedSpeed).lengthSquared() > instantSpeedUpdateThreshold * instantSpeedUpdateThreshold;

    if(instant) {
        widget->ptzController()->continuousMove(data.requestedSpeed);
        data.currentSpeed = data.requestedSpeed;

        m_movementTimer.stop();
    } else {
        if(!m_movementTimer.isActive())
            m_movementTimer.start(speedUpdateIntervalMSec, this);
    }
}

void PtzInstrument::focusMove(QnMediaResourceWidget *widget, qreal speed) {
    widget->ptzController()->continuousFocus(speed);
}

void PtzInstrument::focusAuto(QnMediaResourceWidget *widget) {
    widget->ptzController()->runAuxilaryCommand(Qn::ManualAutoFocusPtzTrait, QString());
}

void PtzInstrument::processPtzClick(const QPointF &pos) {
    if(!target() || m_skipNextAction)
        return;

    /* Don't animate for virtual cameras as it looks bad. */
    if(m_movement != VirtualMovement) {
        QnSplashItem *splashItem = newSplashItem(target());
        splashItem->setSplashType(QnSplashItem::Circular);
        splashItem->setRect(QRectF(0.0, 0.0, 0.0, 0.0));
        splashItem->setPos(pos);
        m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));
    }

    ptzMoveTo(target(), pos);
}

void PtzInstrument::processPtzDrag(const QRectF &rect) {
    if(!target() || m_skipNextAction)
        return;

    QnSplashItem *splashItem = newSplashItem(target());
    splashItem->setSplashType(QnSplashItem::Rectangular);
    splashItem->setPos(rect.center());
    splashItem->setRect(QRectF(-toPoint(rect.size()) / 2, rect.size()));
    m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));

    ptzMoveTo(target(), rect);
}

void PtzInstrument::processPtzDoubleClick() {
    if(!target() || m_skipNextAction)
        return;

    QnSplashItem *splashItem = newSplashItem(target());
    splashItem->setSplashType(QnSplashItem::Rectangular);
    splashItem->setPos(target()->rect().center());
    QSizeF size = target()->size() * 1.1;
    splashItem->setRect(QRectF(-toPoint(size) / 2, size));
    m_activeAnimations.push_back(SplashItemAnimation(splashItem, -1.0, 1.0));

    ptzUnzoom(target());

    /* Also do item unzoom if we're zoomed in. */
    QRectF viewportGeometry = display()->viewportGeometry();
    QRectF zoomedItemGeometry = display()->itemGeometry(target()->item());
    if(viewportGeometry.width() < zoomedItemGeometry.width() * itemUnzoomThreshold || viewportGeometry.height() < zoomedItemGeometry.height() * itemUnzoomThreshold)
        emit doubleClicked(target());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void PtzInstrument::installedNotify() {
    assert(selectionItem() == NULL);
    assert(elementsWidget() == NULL);

    base_type::installedNotify();
}

void PtzInstrument::aboutToBeDisabledNotify() {
    m_clickTimer.stop();
    m_movementTimer.stop();

    base_type::aboutToBeDisabledNotify();
}

void PtzInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(selectionItem())
        delete selectionItem();
    if(elementsWidget())
        delete elementsWidget();
}

bool PtzInstrument::registeredNotify(QGraphicsItem *item) {
    if(!base_type::registeredNotify(item))
        return false;

    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item)) {
        if(widget->resource()) {
            connect(widget, &QnMediaResourceWidget::optionsChanged, this, &PtzInstrument::updateOverlayWidget);
            connect(widget, &QnMediaResourceWidget::fisheyeChanged, this, &PtzInstrument::updateOverlayWidget);

            PtzData &data = m_dataByWidget[widget];
            data.capabilitiesConnection = connect(widget->ptzController(), &QnAbstractPtzController::changed, this, 
                [=](Qn::PtzDataFields fields) { 
                    if(fields & Qn::CapabilitiesPtzField)
                        updateCapabilities(widget); 
                    if(fields & Qn::AuxilaryTraitsPtzField)
                        updateTraits(widget);
                }
            );

            updateCapabilities(widget);
            updateTraits(widget);
            updateOverlayWidgetInternal(widget);

            return true;
        }
    }

    return false;
}

void PtzInstrument::unregisteredNotify(QGraphicsItem *item) {
    base_type::unregisteredNotify(item);

    /* We don't want to use RTTI at this point, so we don't cast to QnMediaResourceWidget. */
    QGraphicsObject *object = item->toGraphicsObject();
    disconnect(object, NULL, this, NULL);

    PtzData &data = m_dataByWidget[object];
    disconnect(data.capabilitiesConnection);
    
    m_dataByWidget.remove(object);
}

void PtzInstrument::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_clickTimer.timerId()) {
        m_clickTimer.stop();

        processPtzClick(m_clickPos);
    } else if(event->timerId() == m_movementTimer.timerId()) { 
        if(!target())
            return;

        ptzMove(target(), m_dataByWidget[target()].requestedSpeed);
        m_movementTimer.stop();
    } else {
        base_type::timerEvent(event);
    }
}

bool PtzInstrument::animationEvent(AnimationEvent *event) {
    qreal dt = event->deltaTime() / 1000.0;
    
    for(int i = m_activeAnimations.size() - 1; i >= 0; i--) {
        SplashItemAnimation &animation = m_activeAnimations[i];

        qreal opacity = animation.item->opacity();
        QRectF rect = animation.item->boundingRect();

        qreal opacitySpeed = animation.fadingIn ? 8.0 : -1.0;

        opacity += dt * opacitySpeed * animation.opacityMultiplier;
        if(opacity >= 1.0) {
            animation.fadingIn = false;
            opacity = 1.0;
        } else if(opacity <= 0.0) {
            animation.item->hide();
            m_freeAnimations.push_back(animation);
            m_activeAnimations.removeAt(i);
            continue;
        }
        qreal ds = dt * m_expansionSpeed * animation.expansionMultiplier;
        rect = rect.adjusted(-ds, -ds, ds, ds);

        animation.item->setOpacity(opacity);
        animation.item->setRect(rect);
    }
    
    return false;
}

bool PtzInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *) {
    m_viewport = viewport;

    return false;
}

bool PtzInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    bool clickTimerWasActive = m_clickTimer.isActive();
    m_clickTimer.stop();

    if(event->button() != Qt::LeftButton) {
        m_skipNextAction = true; /* Interrupted by RMB? Do nothing. */
        reset(); 
        return false;
    }

    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    if(!(target->options() & QnResourceWidget::ControlPtz))
        return false;

    if(!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    PtzManipulatorWidget *manipulator = NULL;
    if(PtzOverlayWidget *overlay = overlayWidget(target)) {
        manipulator = overlay->manipulatorWidget();
        if(!manipulator->isVisible() || !manipulator->rect().contains(manipulator->mapFromItem(item, event->pos())))
            manipulator = NULL;
    }

    const PtzData &data = m_dataByWidget[target];
    if(manipulator) {
        m_movement = ContinuousMovement;

        m_movementOrientations = 0;
        if(data.hasCapabilities(Qn::ContinuousPanCapability))
            m_movementOrientations |= Qt::Horizontal;
        if(data.hasCapabilities(Qn::ContinuousTiltCapability))
            m_movementOrientations |= Qt::Vertical;
    } else {
        if(data.hasCapabilities(Qn::VirtualPtzCapability | Qn::AbsolutePtzCapabilities | Qn::LogicalPositioningPtzCapability)) {
            m_movement = VirtualMovement;
        } else if(data.hasCapabilities(Qn::ViewportPtzCapability)) {
            m_movement = ViewportMovement;
        } else {
            m_movement = NoMovement;
            return false;
        }
    }

    m_skipNextAction = false;
    m_target = target;
    m_isDoubleClick = this->target() == target && clickTimerWasActive && (m_clickPos - event->pos()).manhattanLength() < dragProcessor()->startDragDistance();

    dragProcessor()->mousePressEvent(target, event);
    
    event->accept();
    return false;
}

void PtzInstrument::startDragProcess(DragInfo *) {
    m_isClick = true;
    emit ptzProcessStarted(target());
}

void PtzInstrument::startDrag(DragInfo *) {
    m_isClick = false;
    m_ptzStartedEmitted = false;

    if(!target()) {
        /* Whoops, already destroyed. */
        reset();
        return;
    }

    switch(m_movement) {
    case ContinuousMovement:
        targetManipulator()->setCursor(Qt::BlankCursor);
        target()->setCursor(Qt::BlankCursor);

        ensureElementsWidget();
        opacityAnimator(elementsWidget()->arrowItem())->animateTo(1.0);
        break;
    case ViewportMovement:
        ensureSelectionItem();
        selectionItem()->setParentItem(target());
        selectionItem()->setViewport(m_viewport.data());
        opacityAnimator(selectionItem())->stop();
        selectionItem()->setOpacity(1.0);
        break;
    case VirtualMovement:
        m_pendingMouseReturn = false;
        target()->setCursor(Qt::BlankCursor);
        break;
    default:
        break;
    }

    /* Everything else will be initialized in the first call to drag(). */

    emit ptzStarted(target());
    m_ptzStartedEmitted = true;
}

void PtzInstrument::dragMove(DragInfo *info) {
    if(!target()) {
        reset();
        return;
    }

    switch(m_movement) {
    case ContinuousMovement: {
        QPointF mouseItemPos = info->mouseItemPos();
        QPointF itemCenter = target()->rect().center();

        if(!(m_movementOrientations & Qt::Horizontal))
            mouseItemPos.setX(itemCenter.x());
        if(!(m_movementOrientations & Qt::Vertical))
            mouseItemPos.setY(itemCenter.y());

        QPointF delta = mouseItemPos - itemCenter;
        QSizeF size = target()->size();
        qreal scale = qMax(size.width(), size.height()) / 2.0;
        QPointF speed(qBound(-1.0, delta.x() / scale, 1.0), qBound(-1.0, -delta.y() / scale, 1.0));

        qreal speedMagnitude = length(speed);
        qreal arrowSize = 12.0 * (1.0 + 3.0 * speedMagnitude);

        ensureElementsWidget();
        PtzArrowItem *arrowItem = elementsWidget()->arrowItem();

        arrowItem->moveTo(elementsWidget()->mapFromItem(target(), target()->rect().center()), elementsWidget()->mapFromItem(target(), mouseItemPos));
        arrowItem->setSize(QSizeF(arrowSize, arrowSize));

        ptzMove(target(), QVector3D(speed));
        break;
    }
    case ViewportMovement: 
        ensureSelectionItem();
        selectionItem()->setGeometry(info->mousePressItemPos(), info->mouseItemPos(), aspectRatio(target()->size()), target()->rect());
        break;
    case VirtualMovement: {
        if (m_pendingMouseReturn) {
            if ((info->mouseScreenPos() - info->mousePressScreenPos()).manhattanLength() < 64)
                m_pendingMouseReturn = false;
            else
                QCursor::setPos(info->mousePressScreenPos()); // see comments below
            break;
        }

        QPointF delta = info->mouseItemPos() - info->lastMouseItemPos();

        qreal scale = target()->size().width() / 2.0;
        QPointF shift(delta.x() / scale, -delta.y() / scale);

        QVector3D position;
        target()->ptzController()->getPosition(Qn::LogicalPtzCoordinateSpace, &position);
            
        qreal speed = 0.5 * position.z();
        QVector3D positionDelta(shift.x() * speed, shift.y() * speed, 0.0);
        target()->ptzController()->absoluteMove(Qn::LogicalPtzCoordinateSpace, position + positionDelta, 2.0); /* 2.0 means instant movement. */

        /* Calling setPos on each move event causes serious lags which I so far
         * was unable to explain. This is worked around by invoking it not that frequently. 
         * Note that we don't account for screen-relative position. */
        if(!m_pendingMouseReturn && (info->mouseScreenPos() - info->mousePressScreenPos()).manhattanLength() > 128) {
            m_pendingMouseReturn = true;
            QCursor::setPos(info->mousePressScreenPos()); // TODO: #PTZ #Elric this still looks bad, but not as bad as it looked without this hack.
        }
        break;
    }
    default:
        break;
    }
}

void PtzInstrument::finishDrag(DragInfo *info) {
    if(target()) {
        switch (m_movement) {
        case ContinuousMovement:
            targetManipulator()->setCursor(Qt::SizeAllCursor);
            target()->unsetCursor();

            ensureElementsWidget();
            opacityAnimator(elementsWidget()->arrowItem())->animateTo(0.0);
            break;
        case ViewportMovement: {
            ensureSelectionItem();
            opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

            QRectF selectionRect = selectionItem()->boundingRect();
            QSizeF targetSize = target()->size();

            qreal relativeSize = qMax(selectionRect.width() / targetSize.width(), selectionRect.height() / targetSize.height());
            if(relativeSize >= minPtzZoomRectSize)
                processPtzDrag(selectionRect);
            break;
        }
        case VirtualMovement:
            QCursor::setPos(info->mousePressScreenPos());
            target()->unsetCursor();
            break;
        default:
            break;
        }
    }

    if(m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo *info) {
    if(target()) {
        switch (m_movement) {
        case ContinuousMovement:
            ptzMove(target(), QVector3D(0.0, 0.0, 0.0));
            break;
        case ViewportMovement:
        case VirtualMovement:
            if(m_isClick) {
                if(m_isDoubleClick) {
                    processPtzDoubleClick();
                } else {
                    m_clickTimer.start(m_clickDelayMSec, this);
                    m_clickPos = info->mousePressItemPos();
                }
            }
            break;
        default:
            break;
        }
    }

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_splashItem_destroyed() {
    QnSplashItem *item = static_cast<QnSplashItem *>(sender());

    m_freeAnimations.removeAll(item);
    m_activeAnimations.removeAll(item);
}

void PtzInstrument::at_modeButton_clicked() {
    PtzImageButtonWidget *button = checked_cast<PtzImageButtonWidget *>(sender());

    if(QnMediaResourceWidget *widget = button->target()) {
        ensureOverlayWidget(widget);

        QnItemDewarpingParams iparams = widget->item()->dewarpingParams();
        QnMediaDewarpingParams mparams = widget->dewarpingParams();

        const QList<int> allowed = mparams.allowedPanoFactorValues();

        int idx = (allowed.indexOf(iparams.panoFactor) + 1) % allowed.size();
        iparams.panoFactor = allowed[idx];
        widget->item()->setDewarpingParams(iparams);

        updateOverlayWidgetInternal(widget);
    }
}

void PtzInstrument::at_zoomInButton_pressed() {
    at_zoomButton_activated(1.0);
}

void PtzInstrument::at_zoomInButton_released() {
    at_zoomButton_activated(0.0);
}

void PtzInstrument::at_zoomOutButton_pressed() {
    at_zoomButton_activated(-1.0);
}

void PtzInstrument::at_zoomOutButton_released() {
    at_zoomButton_activated(0.0);
}

void PtzInstrument::at_zoomButton_activated(qreal speed) {
    PtzImageButtonWidget *button = checked_cast<PtzImageButtonWidget *>(sender());
    
    if(QnMediaResourceWidget *widget = button->target())
        ptzMove(widget, QVector3D(0.0, 0.0, speed), true);
}

void PtzInstrument::at_focusInButton_pressed() {
    at_focusButton_activated(1.0);
}

void PtzInstrument::at_focusInButton_released() {
    at_focusButton_activated(0.0);
}

void PtzInstrument::at_focusOutButton_pressed() {
    at_focusButton_activated(-1.0);
}

void PtzInstrument::at_focusOutButton_released() {
    at_focusButton_activated(0.0);
}

void PtzInstrument::at_focusButton_activated(qreal speed) {
    PtzImageButtonWidget *button = checked_cast<PtzImageButtonWidget *>(sender());

    if(QnMediaResourceWidget *widget = button->target())
        focusMove(widget, speed);
}

void PtzInstrument::at_focusAutoButton_clicked() {
    PtzImageButtonWidget *button = checked_cast<PtzImageButtonWidget *>(sender());

    if(QnMediaResourceWidget *widget = button->target())
        focusAuto(widget);
}

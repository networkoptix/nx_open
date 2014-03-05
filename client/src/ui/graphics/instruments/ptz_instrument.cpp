#include "ptz_instrument.h"

#include <cmath>
#include <cassert>
#include <limits>

#include <QtCore/QVariant>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QApplication>

#include <utils/common/executor.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>
#include <utils/math/math.h>
#include <utils/math/space_mapper.h>
#include <utils/math/color_transformations.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <api/media_server_connection.h>

#include <utils/math/coordinate_transformations.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/splash_item.h>
#include <ui/graphics/items/generic/ui_elements_widget.h>
#include <ui/animation/animation_event.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_ptz_camera_watcher.h>

#include "selection_item.h"

//#define QN_PTZ_INSTRUMENT_DEBUG
#ifdef QN_PTZ_INSTRUMENT_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

namespace {
    // TODO: #Elric #customization

    const QColor ptzColor = qnGlobals->ptzColor();

    const QColor ptzArrowBorderColor = toTransparent(ptzColor, 0.75);
    const QColor ptzArrowBaseColor = toTransparent(ptzColor.lighter(120), 0.75);

    const QColor ptzItemBorderColor = toTransparent(ptzColor, 0.75);
    const QColor ptzItemBaseColor = toTransparent(ptzColor.lighter(120), 0.25);

    const qreal instantSpeedUpdateThreshold = 0.1;
    const qreal speedUpdateThreshold = 0.001;
    const int speedUpdateIntervalMSec = 500;

    const double minPtzZoomRectSize = 0.08;

    const qreal itemUnzoomThreshold = 0.975; /* In sync with hardcoded constant in workbench_controller */ // TODO: #Elric

}


// -------------------------------------------------------------------------- //
// PtzArrowItem
// -------------------------------------------------------------------------- //
class PtzArrowItem: public GraphicsPathItem {
    typedef GraphicsPathItem base_type;

public:
    PtzArrowItem(QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_size(QSizeF(32, 32)),
        m_pathValid(false)
    {
        setAcceptedMouseButtons(0);

        setPen(QPen(ptzArrowBorderColor, 0.0));
        setBrush(ptzArrowBaseColor);
    }

    const QSizeF &size() const {
        return m_size;
    }

    void setSize(const QSizeF &size) {
        if(qFuzzyEquals(size, m_size))
            return;

        m_size = size;

        invalidatePath();
    }

    void moveTo(const QPointF &origin, const QPointF &target) {
        setPos(target);
        setRotation(QnGeometry::atan2(origin - target) / M_PI * 180);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = NULL) override {
        ensurePath();

        base_type::paint(painter, option, widget);
    }

protected:
    void invalidatePath() {
        m_pathValid = false;
    }

    void ensurePath() {
        if(m_pathValid)
            return;

        QPainterPath path;
        path.moveTo(0, 0);
        path.lineTo(m_size.height(), m_size.width() / 2);
        path.lineTo(m_size.height(), -m_size.width() / 2);
        path.closeSubpath();

        setPath(path);
        
        QPen pen = this->pen();
        pen.setWidthF(qMax(m_size.width(), m_size.height()) / 16.0);
        setPen(pen);
    }

private:
    /** Arrow size. Height determines arrow length. */
    QSizeF m_size;

    bool m_pathValid;
};


// -------------------------------------------------------------------------- //
// PtzPointerItem
// -------------------------------------------------------------------------- //
class PtzPointerItem: public Animated<GraphicsPathItem>, public AnimationTimerListener {
    typedef Animated<GraphicsPathItem> base_type;

public:
    PtzPointerItem(QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_size(QSizeF(0, 0)),
        m_pathValid(false)
    {
        registerAnimation(this);
        startListening();

        setAcceptedMouseButtons(0);

        setSize(QSizeF(32, 32));
        setBrush(Qt::NoBrush);
        setPen(QPen(ptzItemBorderColor, 0.0));
    }

    const QSizeF &size() const {
        return m_size;
    }

    void setSize(const QSizeF &size) {
        if(qFuzzyEquals(size, m_size))
            return;

        m_size = size;

        setTransformOriginPoint(QnGeometry::toPoint(m_size / 2));
        invalidatePath();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = NULL) override {
        ensurePath();

        base_type::paint(painter, option, widget);
    }

protected:
    virtual void tick(int deltaMSecs) override {
        setRotation(rotation() + deltaMSecs / 1000.0 * 540.0);
    }

    void updatePen() {
        setPen(QPen(ptzItemBorderColor, qMin(m_size.height(), m_size.width()) / 3));
    }

    void invalidatePath() {
        m_pathValid = false;
    }

    void ensurePath() {
        if(m_pathValid)
            return;

        QRectF rect(QPointF(0, 0), m_size);

        QPainterPath path;
        path.arcMoveTo(rect, 0);
        path.arcTo(rect, 0, 90);
        path.arcMoveTo(rect, 180);
        path.arcTo(rect, 180, 90);
        setPath(path);
        
        QPen pen = this->pen();
        pen.setWidthF(qMax(m_size.width(), m_size.height()) / 4.0);
        setPen(pen);
    }

private:
    /** Pointer size. */
    QSizeF m_size;

    bool m_pathValid;
};


// -------------------------------------------------------------------------- //
// PtzImageButtonWidget
// -------------------------------------------------------------------------- //
class PtzImageButtonWidget: public QnTextButtonWidget {
    typedef QnTextButtonWidget base_type;

public:
    PtzImageButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setFrameShape(Qn::EllipticalFrame);
        setRelativeFrameWidth(1.0 / 16.0);
        
        setStateOpacity(0, 0.4);
        setStateOpacity(HOVERED, 0.7);
        setStateOpacity(PRESSED, 1.0);

        setFrameColor(ptzItemBorderColor);
        setWindowColor(ptzItemBaseColor);
    }

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target) {
        m_target = target;
    }

private:
    QPointer<QnMediaResourceWidget> m_target;
};


// -------------------------------------------------------------------------- //
// PtzManipulatorWidget
// -------------------------------------------------------------------------- //
class PtzManipulatorWidget: public GraphicsWidget {
    typedef GraphicsWidget base_type;

public:
    PtzManipulatorWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags) 
    {}

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        QRectF rect = this->rect();
        qreal penWidth = qMin(rect.width(), rect.height()) / 16;
        QPointF center = rect.center();
        QPointF centralStep = QPointF(penWidth, penWidth);

        QnScopedPainterPenRollback penRollback(painter, QPen(ptzItemBorderColor, penWidth));
        Q_UNUSED(penRollback)
        QnScopedPainterBrushRollback brushRollback(painter, ptzItemBaseColor);
        Q_UNUSED(brushRollback)

        painter->drawEllipse(rect);
        painter->drawEllipse(QRectF(center - centralStep, center + centralStep));
    }
};


// -------------------------------------------------------------------------- //
// PtzOverlayWidget
// -------------------------------------------------------------------------- //
class PtzOverlayWidget: public GraphicsWidget {
    Q_DECLARE_TR_FUNCTIONS(PtzOverlayWidget);
    typedef GraphicsWidget base_type;

public:
    PtzOverlayWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags),
        m_markersVisible(true)
    {
        setAcceptedMouseButtons(0);

        /* Note that construction order is important as it defines which items are on top. */
        m_manipulatorWidget = new PtzManipulatorWidget(this);

        m_zoomInButton = new PtzImageButtonWidget(this);
        m_zoomInButton->setIcon(qnSkin->icon("item/ptz_zoom_in.png"));

        m_zoomOutButton = new PtzImageButtonWidget(this);
        m_zoomOutButton->setIcon(qnSkin->icon("item/ptz_zoom_out.png"));

        m_modeButton = new PtzImageButtonWidget(this);
        m_modeButton->setToolTip(tr("Dewarping panoramic mode"));

        updateLayout();
        showCursor();
    }

    void hideCursor() {
        manipulatorWidget()->setCursor(Qt::BlankCursor);
        zoomInButton()->setCursor(Qt::BlankCursor);
        zoomOutButton()->setCursor(Qt::BlankCursor);
    }

    void showCursor() {
        manipulatorWidget()->setCursor(Qt::SizeAllCursor);
        zoomInButton()->setCursor(Qt::ArrowCursor);
        zoomOutButton()->setCursor(Qt::ArrowCursor);
    }

    PtzManipulatorWidget *manipulatorWidget() const {
        return m_manipulatorWidget;
    }

    PtzImageButtonWidget *zoomInButton() const {
        return m_zoomInButton;
    }

    PtzImageButtonWidget *zoomOutButton() const {
        return m_zoomOutButton;
    }

    PtzImageButtonWidget *modeButton() const {
        return m_modeButton;
    }

    bool isMarkersVisible() const {
        return m_markersVisible;
    }

    void setMarkersVisible(bool markersVisible) {
        m_markersVisible = markersVisible;
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyEquals(oldSize, size()))
            updateLayout();
    }

    void setModeButtonText(const QString& text)
    {
        m_modeButton->setText(text);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        if(!m_markersVisible) {
            base_type::paint(painter, option, widget);
            return;
        }

        QRectF rect = this->rect();

        QVector<QPointF> crosshairLines; // TODO: #Elric cache these?

        QPointF center = rect.center();
        qreal d0 = qMin(rect.width(), rect.height()) / 4.0;
        qreal d1 = d0 / 8.0;

        qreal dx = d1 * 3.0;
        while(dx < rect.width() / 2.0) {
            crosshairLines 
                << center + QPointF( dx, d1 / 2.0) << center + QPointF( dx, -d1 / 2.0)
                << center + QPointF(-dx, d1 / 2.0) << center + QPointF(-dx, -d1 / 2.0);
            dx += d1;
        }

        qreal dy = d1 * 3.0;
        while(dy < rect.height() / 2.0) {
            crosshairLines 
                << center + QPointF(d1 / 2.0,  dy) << center + QPointF(-d1 / 2.0,  dy)
                << center + QPointF(d1 / 2.0, -dy) << center + QPointF(-d1 / 2.0, -dy);
            dy += d1;
        }

#if 0
        for(int i = 0; i < 4; i++) {
            qreal a = M_PI * (0.25 + 0.5 * i);
            qreal sin = std::sin(a);
            qreal cos = std::cos(a);
            QPointF v0(cos, sin), v1(-sin, cos);

            crosshairLines
                << center + v0 * d1 << center + v0 * d0
                << center + v0 * d0 + v1 * d1 / 2.0 << center + v0 * d0 - v1 * d1 / 2.0;
        }
#endif

        QnScopedPainterPenRollback penRollback(painter, QPen(ptzItemBorderColor, 0.0));
        painter->drawLines(crosshairLines);
        Q_UNUSED(penRollback)
    }

private:
    void updateLayout() {
        /* We're doing manual layout of child items as this is an overlay widget and
         * we don't want layouts to mess up widget's size constraints. */

        QRectF rect = this->rect();
        QPointF center = rect.center();
        qreal centralWidth = qMin(rect.width(), rect.height()) / 32;
        QPointF xStep(centralWidth, 0), yStep(0, centralWidth);

        m_manipulatorWidget->setGeometry(QRectF(center - xStep - yStep, center + xStep + yStep));
        m_zoomInButton->setGeometry(QRectF(center - xStep * 3 - yStep * 2.5, 1.5 * QnGeometry::toSize(xStep + yStep)));
        m_zoomOutButton->setGeometry(QRectF(center + xStep * 1.5 - yStep * 2.5, 1.5 * QnGeometry::toSize(xStep + yStep)));
        m_modeButton->setGeometry(QRectF((rect.topRight() + rect.bottomRight()) / 2.0 - xStep * 4.0 - yStep * 1.5, 3.0 * QnGeometry::toSize(xStep + yStep)));
    }

private:
    bool m_markersVisible;
    PtzManipulatorWidget *m_manipulatorWidget;
    PtzImageButtonWidget *m_zoomInButton;
    PtzImageButtonWidget *m_zoomOutButton;
    PtzImageButtonWidget *m_modeButton;
};


// -------------------------------------------------------------------------- //
// PtzElementsWidget
// -------------------------------------------------------------------------- //
class PtzElementsWidget: public QnUiElementsWidget {
    typedef QnUiElementsWidget base_type;

public:
    PtzElementsWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setAcceptedMouseButtons(0);

        /* Note that construction order is important as it defines which items are on top. */
        m_arrowItem = new PtzArrowItem(this);
        m_pointerItem = new PtzPointerItem(this);

        m_arrowItem->setOpacity(0.0);
        m_pointerItem->setOpacity(0.0);
        m_pointerItem->setSize(QSizeF(24.0, 24.0));
    }

    PtzArrowItem *arrowItem() const {
        return m_arrowItem;
    }

    PtzPointerItem *pointerItem() const {
        return m_pointerItem;
    }

private:
    PtzArrowItem *m_arrowItem;
    PtzPointerItem *m_pointerItem;
};


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
        result = new QnSplashItem(parentItem);
        result->setColor(toTransparent(ptzColor, 0.5));
        connect(result, SIGNAL(destroyed()), this, SLOT(at_splashItem_destroyed()));
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
    overlay->modeButton()->setTarget(widget);
    overlay->modeButton()->setVisible(isFisheye && isFisheyeEnabled);
    overlay->setMarkersVisible(!isFisheye);

    data.overlayWidget = overlay;

    connect(overlay->zoomInButton(),    SIGNAL(pressed()),  this, SLOT(at_zoomInButton_pressed()));
    connect(overlay->zoomInButton(),    SIGNAL(released()), this, SLOT(at_zoomInButton_released()));
    connect(overlay->zoomOutButton(),   SIGNAL(pressed()),  this, SLOT(at_zoomOutButton_pressed()));
    connect(overlay->zoomOutButton(),   SIGNAL(released()), this, SLOT(at_zoomOutButton_released()));
    connect(overlay->modeButton(),      SIGNAL(clicked()),  this, SLOT(at_modeButton_clicked()));

    widget->addOverlayWidget(overlay, QnResourceWidget::Invisible, true, false, false);

    return overlay;
}

FixedArSelectionItem *PtzInstrument::selectionItem() const {
    return m_selectionItem.data();
}

void PtzInstrument::ensureSelectionItem() {
    if(selectionItem())
        return;

    m_selectionItem = new FixedArSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setPen(QPen(ptzItemBorderColor, 0.0));
    selectionItem()->setBrush(ptzItemBaseColor);
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
    updateOverlayWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void PtzInstrument::updateOverlayWidget(QnMediaResourceWidget *widget) {
    bool hasCrosshair = widget->options() & QnResourceWidget::DisplayCrosshair;

    if(hasCrosshair)
        ensureOverlayWidget(widget);

    QnResourceWidget::OverlayVisibility visibility = hasCrosshair ? QnResourceWidget::AutoVisible : QnResourceWidget::Invisible;

    if(PtzOverlayWidget *overlayWidget = this->overlayWidget(widget)) {
        widget->setOverlayWidgetVisibility(overlayWidget, visibility);

        const PtzData &data = m_dataByWidget[widget];

        bool isFisheye = data.hasCapabilities(Qn::VirtualPtzCapability);
        bool isFisheyeEnabled = widget->dewarpingParams().enabled;

        overlayWidget->manipulatorWidget()->setVisible(data.hasCapabilities(Qn::ContinuousPanTiltCapabilities));
        overlayWidget->zoomInButton()->setVisible(data.hasCapabilities(Qn::ContinuousZoomCapability));
        overlayWidget->zoomOutButton()->setVisible(data.hasCapabilities(Qn::ContinuousZoomCapability));
        
        if (isFisheye) {
            int panoAngle = widget->item()
                    ? 90 * widget->item()->dewarpingParams().panoFactor
                    : 90;
            overlayWidget->modeButton()->setText(QString::number(panoAngle));
        }
        overlayWidget->modeButton()->setVisible(isFisheye && isFisheyeEnabled);
        overlayWidget->setMarkersVisible(!isFisheye);
    }
}

void PtzInstrument::updateCapabilities(QnMediaResourceWidget *widget) {
    PtzData &data = m_dataByWidget[widget];
    
    Qn::PtzCapabilities capabilities = widget->ptzController()->getCapabilities();
    if(data.capabilities == capabilities)
        return;

    data.capabilities = capabilities;
    updateOverlayWidget(widget);
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

void PtzInstrument::processPtzClick(const QPointF &pos) {
    if(!target() || m_skipNextAction)
        return;

    QnSplashItem *splashItem = newSplashItem(target());
    splashItem->setSplashType(QnSplashItem::Circular);
    splashItem->setRect(QRectF(0.0, 0.0, 0.0, 0.0));
    splashItem->setPos(pos);
    m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));

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
            connect(widget, SIGNAL(optionsChanged()), this, SLOT(updateOverlayWidget()));
            connect(widget, SIGNAL(fisheyeChanged()), this, SLOT(updateOverlayWidget()));

            PtzData &data = m_dataByWidget[widget];
            data.capabilitiesConnection = connect(widget->ptzController(), &QnAbstractPtzController::changed, this, 
                [=](Qn::PtzDataFields fields) { 
                    if(fields & Qn::CapabilitiesPtzField) 
                        updateCapabilities(widget); 
                }
            );

            updateCapabilities(widget);
            updateOverlayWidget(widget);

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

    if(manipulator) {
        m_movement = ContinuousMovement;
    } else {
        const PtzData &data = m_dataByWidget[target];
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
        QPointF delta = info->mouseItemPos() - target()->rect().center();
        QSizeF size = target()->size();
        qreal scale = qMax(size.width(), size.height()) / 2.0;
        QPointF speed(qBound(-1.0, delta.x() / scale, 1.0), qBound(-1.0, -delta.y() / scale, 1.0));

        qreal speedMagnitude = length(speed);
        qreal arrowSize = 12.0 * (1.0 + 3.0 * speedMagnitude);

        ensureElementsWidget();
        PtzArrowItem *arrowItem = elementsWidget()->arrowItem();
        arrowItem->moveTo(elementsWidget()->mapFromItem(target(), target()->rect().center()), elementsWidget()->mapFromItem(target(), info->mouseItemPos()));
        arrowItem->setSize(QSizeF(arrowSize, arrowSize));

        ptzMove(target(), QVector3D(speed));
        break;
    }
    case ViewportMovement: 
        ensureSelectionItem();
        selectionItem()->setGeometry(info->mousePressItemPos(), info->mouseItemPos(), aspectRatio(target()->size()), target()->rect());
        break;
    case VirtualMovement:
        if(info->mouseScreenPos() != info->mousePressScreenPos()) {
            QPointF delta = info->mouseItemPos() - info->lastMouseItemPos();
            qreal scale = target()->size().width() / 2.0;
            QPointF shift(delta.x() / scale, -delta.y() / scale);

            QVector3D position;
            target()->ptzController()->getPosition(Qn::LogicalPtzCoordinateSpace, &position);
            
            qreal speed = 0.5 * position.z();
            QVector3D positionDelta(shift.x() * speed, shift.y() * speed, 0.0);
            target()->ptzController()->absoluteMove(Qn::LogicalPtzCoordinateSpace, position + positionDelta, 2.0); /* 2.0 means instant movement. */

            /* Calling setPos on each move event causes serious lags which I've so far
             * was unable to explain. This is worked around by invoking it not that frequently. 
             * Note that we don't account for screen-relative position here. */
            if((info->mouseScreenPos() - info->mousePressScreenPos()).manhattanLength() > 128)
                QCursor::setPos(info->mousePressScreenPos()); // TODO: #PTZ #Elric this still looks bad, but not as bad as it looked without this hack.
        }
        break;
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

        updateOverlayWidget(widget);
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

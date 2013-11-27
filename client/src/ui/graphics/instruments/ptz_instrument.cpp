#include "ptz_instrument.h"

#include <cmath>
#include <cassert>
#include <limits>

#include <QtCore/QVariant>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QApplication>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>
#include <utils/math/math.h>
#include <utils/math/space_mapper.h>
#include <utils/math/color_transformations.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

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
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_ptz_camera_watcher.h>
#include <ui/fisheye/fisheye_ptz_controller.h>

#include "selection_item.h"

//#define QN_PTZ_INSTRUMENT_DEBUG
#ifdef QN_PTZ_INSTRUMENT_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

namespace {
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

        setPen(ptzArrowBorderColor);
        setBrush(ptzArrowBaseColor);
    }

    const QSizeF &size() const {
        return m_size;
    }

    void setSize(const QSizeF &size) {
        if(qFuzzyCompare(size, m_size))
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
        setPen(ptzItemBorderColor);
    }

    const QSizeF &size() const {
        return m_size;
    }

    void setSize(const QSizeF &size) {
        if(qFuzzyCompare(size, m_size))
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
        m_modeButton->setText(lit("90"));
        m_modeButton->setToolTip(lit("Dewarping panoramic mode"));

        updateLayout();
    }

    void hideCursor()
    {
        manipulatorWidget()->setCursor(Qt::BlankCursor);
        zoomInButton()->setCursor(Qt::BlankCursor);
        zoomOutButton()->setCursor(Qt::BlankCursor);
    }

    void showCursor()
    {
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

        if(!qFuzzyCompare(oldSize, size()))
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

        QnScopedPainterPenRollback penRollback(painter, QPen(ptzItemBorderColor));
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
        makeSet(QEvent::GraphicsSceneMouseDoubleClick, QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), 
        parent
    ),
    QnWorkbenchContextAware(parent),
    m_clickDelayMSec(QApplication::doubleClickInterval()),
    m_expansionSpeed(qnGlobals->workbenchUnitSize() / 5.0)
{
    connect(display(), SIGNAL(resourceAdded(const QnResourcePtr &)), this, SLOT(at_display_resourceAdded(const QnResourcePtr &)));
    connect(display(), SIGNAL(resourceAboutToBeRemoved(const QnResourcePtr &)), this, SLOT(at_display_resourceAboutToBeRemoved(const QnResourcePtr &)));
}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();
}


QnMediaResourceWidget *PtzInstrument::target() const {
    return m_target.data();
}

PtzManipulatorWidget *PtzInstrument::manipulator() const {
    return m_manipulator.data();
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

    PtzOverlayWidget *overlay = new PtzOverlayWidget();
    overlay->setOpacity(0.0);
    overlay->manipulatorWidget()->setCursor(Qt::SizeAllCursor);
    overlay->zoomInButton()->setTarget(widget);
    overlay->zoomOutButton()->setTarget(widget);
    //overlay->modeButton()->setTarget(widget);
    //overlay->modeButton()->setVisible(widget->virtualPtzController() != NULL);
    //overlay->setMarkersVisible(widget->virtualPtzController() == NULL);
    overlay->modeButton()->setVisible(false); // TODO: #PTZ

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
    selectionItem()->setPen(ptzItemBorderColor);
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

    if(PtzOverlayWidget *staticOverlay = overlayWidget(widget)) {
        widget->setOverlayWidgetVisibility(staticOverlay, visibility);

        staticOverlay->manipulatorWidget()->setVisible((m_dataByWidget[widget].capabilities & Qn::ContinuousPanTiltCapabilities) == Qn::ContinuousPanTiltCapabilities);
        
        //if (widget->virtualPtzController())
            //staticOverlay->setModeButtonText(widget->virtualPtzController()->getPanoModeText());
    }
}

void PtzInstrument::updateCapabilities(const QnResourcePtr &resource) {
    foreach(QnResourceWidget *widget, display()->widgets(resource))
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
            updateCapabilities(mediaWidget);
}

void PtzInstrument::updateCapabilities(QnMediaResourceWidget *widget) {
    PtzData &data = m_dataByWidget[widget];
    Qn::PtzCapabilities oldCapabilities = data.capabilities;

    data.capabilities = widget->ptzController()->getCapabilities();

    if(oldCapabilities != data.capabilities)
        updateOverlayWidget(widget);
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos) {
    ptzMoveTo(widget, QRectF(pos - toPoint(widget->size() / 2), widget->size()));
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect) {
    QVector3D newPhysicalPosition = physicalPositionForRect(widget, rect);
    if(qIsNaN(newPhysicalPosition)) {
        //m_ptzController->setMovement(widget, QVector3D());
        PtzData &data = m_dataByWidget[widget];
        data.pendingAbsoluteMove = rect;
        return; 
    }

    TRACE("PTZ ZOOM(" << newPhysicalPosition.x() - oldPhysicalPosition.x() << ", " << newPhysicalPosition.y() - oldPhysicalPosition.y() << ", " << zoom << "x)");
    //m_ptzController->setPhysicalPosition(widget, newPhysicalPosition);
}

QVector3D PtzInstrument::physicalPositionForPos(QnMediaResourceWidget *widget, const QPointF &pos) {
    return physicalPositionForRect(widget, QRectF(pos - toPoint(widget->size() / 2), widget->size()));
}

QVector3D PtzInstrument::physicalPositionForRect(QnMediaResourceWidget *widget, const QRectF &rect) 
{
    return QVector3D();
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
        data.pendingAbsoluteMove = QRectF();

        m_movementTimer.stop();
    } else {
        if(!m_movementTimer.isActive())
            m_movementTimer.start(speedUpdateIntervalMSec, this);
    }
}

void PtzInstrument::processPtzClick(const QPointF &pos) {
    if(!target() || m_skipNextAction)
        return;

    /*if (!target()->virtualPtzController()) {
        // built in virtual PTZ execute command too fast. animation looks bad
        QnSplashItem *splashItem = newSplashItem(target());
        splashItem->setSplashType(QnSplashItem::Circular);
        splashItem->setRect(QRectF(0.0, 0.0, 0.0, 0.0));
        splashItem->setPos(pos);
        m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));
    } else {
        target()->virtualPtzController()->setAnimationEnabled(true);
    }*/

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

#if 0
    if (!target()->virtualPtzController()) {
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
    } else {
        emit doubleClicked(target());
    }
#endif
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

            PtzData &data = m_dataByWidget[widget];
            //data.currentSpeed = data.requestedSpeed = m_ptzController->movement(widget);

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

bool PtzInstrument::mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mousePressEvent(item, event);
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

    if(!manipulator && !(m_dataByWidget[target].capabilities & Qn::AbsolutePtzCapabilities))
        return false;

    m_skipNextAction = false;
    m_isDoubleClick = this->target() == target && clickTimerWasActive && (m_clickPos - event->pos()).manhattanLength() < dragProcessor()->startDragDistance();
    m_target = target;
    m_manipulator = manipulator;

    //ptzUpdate(target);
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

    if(manipulator()) {
        manipulator()->setCursor(Qt::BlankCursor);
        target()->setCursor(Qt::BlankCursor);

        ensureElementsWidget();
        opacityAnimator(elementsWidget()->arrowItem())->animateTo(1.0);
        /* Everything else will be initialized in the first call to drag(). */
    } /*else if (target()->virtualPtzController()) {
        target()->virtualPtzController()->setAnimationEnabled(false);
        target()->setCursor(Qt::BlankCursor);
    }*/ else {
        ensureSelectionItem();
        selectionItem()->setParentItem(target());
        selectionItem()->setViewport(m_viewport.data());
        opacityAnimator(selectionItem())->stop();
        selectionItem()->setOpacity(1.0);
        /* Everything else will be initialized in the first call to drag(). */
    }

    emit ptzStarted(target());
    m_ptzStartedEmitted = true;
}

void PtzInstrument::dragMove(DragInfo *info) {
    if(!target()) {
        reset();
        return;
    }

    if(manipulator()) {
        QPointF delta = info->mouseItemPos() - target()->rect().center();
        QSizeF size = target()->size();
        qreal scale = qMax(size.width(), size.height()) / 2.0;
        QPointF speed(qBound(-1.0, delta.x() / scale, 1.0), qBound(-1.0, -delta.y() / scale, 1.0));

        /* Adjust speed in case we're dealing with octagonal ptz. */
        const PtzData &data = m_dataByWidget[target()];

#if 0
        if(data.capabilities & Qn::OctagonalPtzCapability) {
            QnPolarPoint<double> polarSpeed = cartesianToPolar(speed);
            polarSpeed.alpha = qRound(polarSpeed.alpha, M_PI / 4.0); /* Rounded to 45 degrees. */
            speed = polarToCartesian<QPointF>(polarSpeed.r, polarSpeed.alpha);
            if(qFuzzyIsNull(speed.x())) // TODO: #Elric use lower null threshold
                speed.setX(0.0);
            if(qFuzzyIsNull(speed.y()))
                speed.setY(0.0);
            // TODO: #Elric rebound to 1.0
        }
#endif

        qreal speedMagnitude = length(speed);
        qreal arrowSize = 12.0 * (1.0 + 3.0 * speedMagnitude);

        ensureElementsWidget();
        PtzArrowItem *arrowItem = elementsWidget()->arrowItem();
        arrowItem->moveTo(elementsWidget()->mapFromItem(target(), target()->rect().center()), elementsWidget()->mapFromItem(target(), info->mouseItemPos()));
        arrowItem->setSize(QSizeF(arrowSize, arrowSize));

        ptzMove(target(), QVector3D(speed));
    } 
#if 0
    else if (target()->virtualPtzController()) {
        QCursor::setPos(info->mousePressScreenPos());
        if(info->mouseScreenPos() != info->mousePressScreenPos()) {
            QPointF delta = info->mouseItemPos() - info->lastMouseItemPos();
            QSizeF size = target()->size();
            qreal scale = qMax(size.width(), size.height()) / 2.0;
            QPointF shift(delta.x() / scale, -delta.y() / scale);

            //QVector3D position = m_ptzController->physicalPosition(target());
            /*qreal speed = 10.0 * mm35EquivToFov(position.z());
            QVector3D positionDelta(shift.x() * speed, shift.y() * speed, 0.0);
            m_ptzController->setPhysicalPosition(target(), position + positionDelta);*/
        }
    } 
#endif
    else {
        ensureSelectionItem();
        selectionItem()->setGeometry(info->mousePressItemPos(), info->mouseItemPos(), aspectRatio(target()->size()), target()->rect());
    }
}

void PtzInstrument::finishDrag(DragInfo * info) {
    Q_UNUSED(info)
    if(target()) {
        if(manipulator()) {
            manipulator()->setCursor(Qt::SizeAllCursor);
            target()->unsetCursor();

            ensureElementsWidget();
            opacityAnimator(elementsWidget()->arrowItem())->animateTo(0.0);
        } /*else if (target()->virtualPtzController()) {
            target()->unsetCursor();
        } */else {
            ensureSelectionItem();
            opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

            QRectF selectionRect = selectionItem()->boundingRect();
            QSizeF targetSize = target()->size();

            qreal relativeSize = qMax(selectionRect.width() / targetSize.width(), selectionRect.height() / targetSize.height());
            if(relativeSize >= minPtzZoomRectSize)
                processPtzDrag(selectionRect);
        }
    }

    if(m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo *info) {
    if(target()) {
        if(!manipulator() && m_isClick && m_isDoubleClick) {
            processPtzDoubleClick();
        } else if(!manipulator() && m_isClick) {
            m_clickTimer.start(m_clickDelayMSec, this);
            m_clickPos = info->mousePressItemPos();
        } else if(manipulator()) {
            ptzMove(target(), QVector3D(0.0, 0.0, 0.0));
        }
    }

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_display_resourceAdded(const QnResourcePtr &resource) {
    connect(resource, SIGNAL(ptzCapabilitiesChanged(const QnResourcePtr &)), this, SLOT(updateCapabilities(const QnResourcePtr  &)));
}

void PtzInstrument::at_display_resourceAboutToBeRemoved(const QnResourcePtr &resource) {
    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        disconnect(camera, NULL, this, NULL);
}

void PtzInstrument::at_ptzController_positionChanged(const QnMediaResourceWidget* widget) {
    if(!target() || target() != widget)
        return;

    PtzData &data = m_dataByWidget[target()];
    QRectF rect = data.pendingAbsoluteMove;
    if(rect.isNull())
        return;

    data.pendingAbsoluteMove = QRectF();
    ptzMoveTo(target(), rect);
}

void PtzInstrument::at_mapperWatcher_mapperChanged(const QnVirtualCameraResourcePtr &resource) {
    updateCapabilities(resource);
}

void PtzInstrument::at_splashItem_destroyed() {
    QnSplashItem *item = static_cast<QnSplashItem *>(sender());

    m_freeAnimations.removeAll(item);
    m_activeAnimations.removeAll(item);
}

void PtzInstrument::at_modeButton_clicked() {
    PtzImageButtonWidget *button = checked_cast<PtzImageButtonWidget *>(sender());

    if(QnMediaResourceWidget *widget = button->target()) {
        //m_ptzController->changePanoMode(widget);
        ensureOverlayWidget(widget);
        //overlayWidget(widget)->setModeButtonText(m_ptzController->getPanoModeText(widget));
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

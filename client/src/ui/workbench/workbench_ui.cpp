#include "workbench_ui.h"
#include <cassert>
#include <cmath> /* For std::floor. */

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QStyle>

#include <utils/common/event_signalizator.h>

#include <camera/resource_display.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/widget_opacity_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/ui_elements_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/activity_listener_instrument.h>
#include <ui/graphics/items/image_button_widget.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/masked_proxy_widget.h>

#include <ui/processors/hover_processor.h>

#include <ui/videoitem/navigationitem.h>
#include <ui/navigationtreewidget.h>
#include <ui/context_menu_helper.h>
#include <ui/skin/skin.h>

#include "workbench.h"
#include "workbench_display.h"
#include "core/resource/security_cam_resource.h"

Q_DECLARE_METATYPE(VariantAnimator *)

namespace {
    const qreal normalTreeOpacity = 0.85;
    const qreal hoverTreeOpacity = 0.95;

    const qreal normalTreeBackgroundOpacity = 0.5;
    const qreal hoverTreeBackgroundOpacity = 1.0;

    const qreal normalSliderOpacity = 0.5;
    const qreal hoverSliderOpacity = 0.95;

    const qreal normalTitleOpacity = 0.5;
    const qreal hoverTitleOpacity = 0.95;

    const int hideConstrolsTimeoutMSec = 2000;

} // anonymous namespace


QnWorkbenchUi::QnWorkbenchUi(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_display(display),
    m_manager(display->instrumentManager()),
    m_treePinned(false),
    m_inactive(false)
{
    memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    /* Install and configure instruments. */
    m_uiElementsInstrument = new UiElementsInstrument(this);
    m_controlsActivityInstrument = new ActivityListenerInstrument(hideConstrolsTimeoutMSec, this);
    m_manager->installInstrument(m_uiElementsInstrument, InstallationMode::INSTALL_BEFORE, m_display->paintForwardingInstrument());
    m_manager->installInstrument(m_controlsActivityInstrument);

    m_controlsActivityInstrument->recursiveDisable();

    connect(m_controlsActivityInstrument, SIGNAL(activityStopped()),                                                                this,                           SLOT(at_activityStopped()));
    connect(m_controlsActivityInstrument, SIGNAL(activityResumed()),                                                                this,                           SLOT(at_activityStarted()));


    /* Create controls. */
    m_controlsWidget = m_uiElementsInstrument->widget();
    m_controlsWidget->setFlag(QGraphicsItem::ItemIsPanel);
    m_display->setLayer(m_controlsWidget, QnWorkbenchDisplay::UI_ELEMENTS_LAYER);

    QnSingleEventSignalizator *deactivationSignalizator = new QnSingleEventSignalizator(this);
    deactivationSignalizator->setEventType(QEvent::WindowDeactivate);
    m_controlsWidget->installEventFilter(deactivationSignalizator);

    connect(deactivationSignalizator,   SIGNAL(activated(QObject *, QEvent *)),                                                     this,                           SLOT(at_controlsWidget_deactivated()));
    connect(m_controlsWidget,           SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_controlsWidget_geometryChanged()));

    /* Tree widget. */
    m_treeWidget = new NavigationTreeWidget();
    m_treeWidget->setAttribute(Qt::WA_TranslucentBackground);
    {
        QPalette palette = m_treeWidget->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        m_treeWidget->setPalette(palette);
    }

    connect(&cm_showNavTree, SIGNAL(triggered()), this, SLOT(toggleTreeVisible()));

    m_treeBackgroundItem = new QGraphicsWidget(m_controlsWidget);
    m_treeBackgroundItem->setAutoFillBackground(true);
    {
        QPalette palette = m_treeBackgroundItem->palette();

        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0,  QColor(0, 0, 0, 255));
        gradient.setColorAt(0.99, QColor(0, 0, 0, 64));
        gradient.setColorAt(1.0,  QColor(0, 0, 0, 255));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_treeBackgroundItem->setPalette(palette);
    }
    m_treeBackgroundItem->setOpacity(normalTreeBackgroundOpacity);

    m_treeItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_treeItem->setWidget(m_treeWidget);
    m_treeItem->setFocusPolicy(Qt::StrongFocus);
    m_treeItem->setOpacity(normalTreeOpacity);

    m_treePinButton = new QnImageButtonWidget(m_controlsWidget);
    m_treePinButton->resize(24, 24);
    m_treePinButton->setOpacity(normalTreeBackgroundOpacity);
    m_treePinButton->setPixmap(QnImageButtonWidget::DEFAULT, Skin::pixmap("pin.png"));
    m_treePinButton->setPixmap(QnImageButtonWidget::CHECKED, Skin::pixmap("unpin.png"));
    m_treePinButton->setCheckable(true);

    m_treeShowButton = new QnImageButtonWidget(m_controlsWidget);
    m_treeShowButton->resize(15, 45);
    m_treeShowButton->setOpacity(normalTreeBackgroundOpacity);
    m_treeShowButton->setPixmap(QnImageButtonWidget::DEFAULT, Skin::pixmap("slide_right.png"));
    m_treeShowButton->setPixmap(QnImageButtonWidget::HOVERED, Skin::pixmap("slide_right_hovered.png"));
    m_treeShowButton->setPixmap(QnImageButtonWidget::CHECKED, Skin::pixmap("slide_left.png"));
    m_treeShowButton->setPixmap(QnImageButtonWidget::CHECKED | QnImageButtonWidget::HOVERED, Skin::pixmap("slide_left_hovered.png"));
    m_treeShowButton->setCheckable(true);
    m_treeShowButton->setAnimationSpeed(4.0);

    m_treeOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeOpacityProcessor->addTargetItem(m_treeItem);
    m_treeOpacityProcessor->addTargetItem(m_treeShowButton);

    m_treeHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeHidingProcessor->addTargetItem(m_treeItem);
    m_treeHidingProcessor->addTargetItem(m_treeShowButton);
    m_treeHidingProcessor->setHoverLeaveDelay(1000);
    m_treeHidingProcessor->setFocusLeaveDelay(1000);

    m_treeShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeShowingProcessor->addTargetItem(m_treeShowButton);
    m_treeShowingProcessor->setHoverEnterDelay(250);

    m_treeXAnimator = new VariantAnimator(this);
    m_treeXAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_treeXAnimator->setTargetObject(m_treeItem);
    m_treeXAnimator->setAccessor(new PropertyAccessor("x"));
    m_treeXAnimator->setSpeed(m_treeItem->size().width() * 2.0);
    m_treeXAnimator->setTimeLimit(500);

    m_treeOpacityAnimatorGroup = new AnimatorGroup(this);
    m_treeOpacityAnimatorGroup->setTimer(display->animationInstrument()->animationTimer());
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeItem));
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeShowButton));

    setTreeVisible(false, false);

    connect(m_treePinButton,            SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_treePinButton_toggled(bool)));
    connect(m_treeShowButton,           SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_treeShowButton_toggled(bool)));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(at_treeOpacityProcessor_hoverLeft()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_treeOpacityProcessor_hoverEntered()));
    connect(m_treeHidingProcessor,      SIGNAL(hoverFocusLeft()),                                                                   this,                           SLOT(at_treeHidingProcessor_hoverFocusLeft()));
    connect(m_treeShowingProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_treeShowingProcessor_hoverEntered()));
    connect(m_treeItem,                 SIGNAL(paintRectChanged()),                                                                 this,                           SLOT(at_treeItem_paintGeometryChanged()));
    connect(m_treeItem,                 SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_treeItem_paintGeometryChanged()));

    /* Navigation slider. */
    m_sliderItem = new NavigationItem(m_controlsWidget);
    m_sliderItem->setOpacity(normalSliderOpacity);

    m_sliderOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem);

    m_sliderYAnimator = new VariantAnimator(this);
    m_sliderYAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_sliderYAnimator->setTargetObject(m_sliderItem);
    m_sliderYAnimator->setAccessor(new PropertyAccessor("y"));
    m_sliderYAnimator->setSpeed(m_sliderItem->size().height() * 2.0);
    m_sliderYAnimator->setTimeLimit(500);

    setSliderVisible(false, false);

    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_sliderOpacityProcessor_hoverEntered()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                                                                        this,                           SLOT(at_sliderOpacityProcessor_hoverLeft()));
    connect(m_sliderItem,               SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_navigationItem_geometryChanged()));
    connect(m_sliderItem,               SIGNAL(actualCameraChanged(CLVideoCamera *)),                                               this,                           SLOT(at_navigationItem_actualCameraChanged(CLVideoCamera *)));
    //connect(m_sliderItem,           SIGNAL(playbackMaskChanged(const QnTimePeriodList &)),                                      m_display,                      SIGNAL(playbackMaskChanged(const QnTimePeriodList &)));
    connect(m_sliderItem,               SIGNAL(enableItemSync(bool)),                                                               m_display,                      SIGNAL(enableItemSync(bool)));
    connect(m_display,                  SIGNAL(displayingStateChanged(QnResourcePtr, bool)),                                        m_sliderItem,               SLOT(onDisplayingStateChanged(QnResourcePtr, bool)));


    /* Title bar. */
    m_titleItem = new QGraphicsWidget(m_controlsWidget);
    m_titleItem->setAutoFillBackground(true);
    {
        QPalette palette = m_titleItem->palette();

        QLinearGradient gradient(0, 0, 0, 1);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0,  QColor(0, 0, 0, 255));
        gradient.setColorAt(0.99, QColor(0, 0, 0, 64));
        gradient.setColorAt(1.0,  QColor(0, 0, 0, 255));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_titleItem->setPalette(palette);
    }
    m_titleItem->setOpacity(normalTitleOpacity);
    m_titleItem->resize(100, 100);
    m_titleItem->setPos(0.0, -100.0);

    m_titleYAnimator = new VariantAnimator(this);
    m_titleYAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_titleYAnimator->setTargetObject(m_titleItem);
    m_titleYAnimator->setAccessor(new PropertyAccessor("y"));
    m_titleYAnimator->setSpeed(m_sliderItem->size().height() * 2.0);
    m_titleYAnimator->setTimeLimit(500);

    /* Connect to display. */ 
    connect(m_display,                  SIGNAL(widgetChanged(QnWorkbench::ItemRole)),                                               this,                           SLOT(at_display_widgetChanged(QnWorkbench::ItemRole)));
    connect(m_display,                  SIGNAL(widgetAdded(QnResourceWidget *)),                                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(m_display,                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
}

QnWorkbenchUi::~QnWorkbenchUi() {
    return;
}

QnWorkbenchDisplay *QnWorkbenchUi::display() const {
    return m_display;
}

QnWorkbench *QnWorkbenchUi::workbench() const {
    return m_display->workbench();
}

void QnWorkbenchUi::setTreeVisible(bool visible, bool animate)
{
    m_visibility.treeVisible = visible;
    m_treeShowButton->setChecked(visible);

    qreal newX = visible ? 0.0 : -m_treeItem->size().width() - 1.0 /* Just in case. */;
    if (animate) {
        m_treeXAnimator->animateTo(newX);
    } else {
        m_treeItem->setX(newX);
    }
}

void QnWorkbenchUi::setSliderVisible(bool visible, bool animate) {
    m_visibility.sliderVisible = visible;

    qreal newY = m_controlsWidget->size().height() + (visible ? -m_sliderItem->size().height() : 32.0 /* So that tooltips are not visible. */);
    if (animate)
        m_sliderYAnimator->animateTo(newY);
    else
        m_sliderItem->setY(newY);
}

void QnWorkbenchUi::toggleTreeVisible() {
    setTreeVisible(!m_visibility.treeVisible);
}


QRectF QnWorkbenchUi::updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry) {
    QRectF controlRect = m_controlsWidget->rect();
    
    QPointF pos(
        treeGeometry.x(),
        qMax(titleGeometry.bottom(), 0.0)
    );
    QSizeF size(
        treeGeometry.width(),
        qMin(sliderGeometry.y(), controlRect.bottom()) - pos.y()
    );
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateTreeGeometry() {
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedTreeGeometry(m_treeItem->geometry(), m_titleItem->geometry(), m_sliderItem->geometry());
    m_treeItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));
    
    /* Always change position. */
    m_treeItem->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if(m_sliderYAnimator->isRunning()) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_sliderYAnimator->targetValue().toReal());
        defer |= !qFuzzyCompare(sliderPos, m_sliderItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        sliderPos = m_sliderItem->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if(m_titleYAnimator->isRunning()) {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyCompare(titlePos, m_titleItem->pos());
    } else {
        titlePos = m_titleItem->pos();
    }

    /* Calculate target geometry. */
    geometry = updatedTreeGeometry(m_treeItem->geometry(), QRectF(titlePos, m_titleItem->size()), QRectF(sliderPos, m_sliderItem->size()));
    if(qFuzzyCompare(geometry, m_treeItem->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if(defer && geometry.height() < m_treeItem->size().height())
        return;

    m_treeItem->resize(geometry.size());
}

void QnWorkbenchUi::updateViewportMargins() {
    m_display->setViewportMargins(QMargins(
        m_treePinned ? std::floor(qMax(0.0, m_treeItem->pos().x() + m_treeItem->size().width())) : 0.0,
        0.0,
        0.0,
        std::floor(qMax(0.0, m_controlsWidget->size().height() - m_sliderItem->pos().y()))
    ));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchUi::at_display_widgetChanged(QnWorkbench::ItemRole role) {
    QnResourceWidget *widget = m_display->widget(role);
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    m_widgetByRole[role] = widget;

    /* Tune activity listener instrument. */
    if(role == QnWorkbench::ZOOMED) {
        if(oldWidget == NULL)
            m_controlsActivityInstrument->recursiveEnable();
        if(widget == NULL)
            m_controlsActivityInstrument->recursiveDisable();
    }

    /* Update navigation item's target. */
    QnResourceWidget *targetWidget = m_widgetByRole[QnWorkbench::ZOOMED];
    if(targetWidget == NULL)
        targetWidget = m_widgetByRole[QnWorkbench::RAISED];
    m_sliderItem->setVideoCamera(targetWidget == NULL ? NULL : targetWidget->display()->camera());
}

void QnWorkbenchUi::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
#ifndef DEBUG_MOTION
    if(cameraResource != NULL) 
#endif
    {
        connect(widget, SIGNAL(motionRegionSelected(QnResourcePtr, QnAbstractArchiveReader*, QRegion)), m_sliderItem, SLOT(loadMotionPeriods(QnResourcePtr, QnAbstractArchiveReader*, QRegion)));
        connect(m_sliderItem, SIGNAL(clearMotionSelection()), widget, SLOT(clearMotionSelection()));
        m_sliderItem->addReserveCamera(widget->display()->camera());
    }
}

void QnWorkbenchUi::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
    if(cameraResource != NULL)
        m_sliderItem->removeReserveCamera(widget->display()->camera());
}

void QnWorkbenchUi::at_controlsWidget_deactivated() {
    /* Re-activate it. */
    display()->scene()->setActiveWindow(m_controlsWidget);
}

void QnWorkbenchUi::at_controlsWidget_geometryChanged() {
    QGraphicsWidget *controlsWidget = m_controlsWidget;
    QSizeF size = controlsWidget->size();
    if(qFuzzyCompare(m_controlsWidgetSize, size))
        return;
    QSizeF oldSize = m_controlsWidgetSize;
    m_controlsWidgetSize = size;

    /* We lay everything out manually. */

    m_sliderItem->setGeometry(QRectF(
        0.0,
        m_sliderItem->pos().y() - oldSize.height() + size.height(),
        size.width(),
        m_sliderItem->size().height()
    ));

    m_titleItem->setGeometry(QRectF(
        0.0,
        m_titleItem->pos().y(),
        size.width(),
        m_titleItem->size().height()
    ));

    updateTreeGeometry();
}

void QnWorkbenchUi::at_navigationItem_geometryChanged() {
    updateTreeGeometry();

    updateViewportMargins();
}

void QnWorkbenchUi::at_navigationItem_actualCameraChanged(CLVideoCamera *camera) {
    setSliderVisible(camera != NULL, true);
}

void QnWorkbenchUi::at_sliderOpacityProcessor_hoverEntered() {
    opacityAnimator(m_sliderItem)->animateTo(hoverSliderOpacity);
}

void QnWorkbenchUi::at_sliderOpacityProcessor_hoverLeft() {
    opacityAnimator(m_sliderItem)->animateTo(normalSliderOpacity);
}

void QnWorkbenchUi::at_treeItem_paintGeometryChanged() {
    QRectF paintGeometry = m_treeItem->paintGeometry();

    /* Don't hide tree item here. It will repaint itself when shown, which will
     * degrade performance. */

    m_treeBackgroundItem->setGeometry(paintGeometry);
    m_treeShowButton->setPos(QPointF(
        paintGeometry.right(),
        (paintGeometry.top() + paintGeometry.bottom() - m_treeShowButton->size().height()) / 2
    ));
    m_treePinButton->setPos(QPointF(
        paintGeometry.right() - m_treePinButton->size().width(),
        paintGeometry.top()
    ));

    updateViewportMargins();
}

void QnWorkbenchUi::at_treeHidingProcessor_hoverFocusLeft() {
    if(!m_treePinned) 
        setTreeVisible(false);
}

void QnWorkbenchUi::at_treeShowingProcessor_hoverEntered() {
    if(!m_treePinned) 
        setTreeVisible(true);
    m_treeHidingProcessor->forceHoverEnter();
    m_treeOpacityProcessor->forceHoverEnter();
}

void QnWorkbenchUi::at_treeOpacityProcessor_hoverLeft() {
    m_treeOpacityAnimatorGroup->pause();
    opacityAnimator(m_treeItem)->setTargetValue(normalTreeOpacity);
    opacityAnimator(m_treeBackgroundItem)->setTargetValue(normalTreeBackgroundOpacity);
    opacityAnimator(m_treeShowButton)->setTargetValue(normalTreeBackgroundOpacity);
    m_treeOpacityAnimatorGroup->start();
}

void QnWorkbenchUi::at_treeOpacityProcessor_hoverEntered() {
    m_treeOpacityAnimatorGroup->pause();
    opacityAnimator(m_treeItem)->setTargetValue(hoverTreeOpacity);
    opacityAnimator(m_treeBackgroundItem)->setTargetValue(hoverTreeBackgroundOpacity);
    opacityAnimator(m_treeShowButton)->setTargetValue(hoverTreeBackgroundOpacity);
    m_treeOpacityAnimatorGroup->start();
}

void QnWorkbenchUi::at_treeShowButton_toggled(bool checked) {
    m_treeShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */
    setTreeVisible(checked);
}

void QnWorkbenchUi::at_treePinButton_toggled(bool checked) {
    m_treePinned = checked;
    
    if(checked)
        setTreeVisible(true);

    updateViewportMargins();
}

void QnWorkbenchUi::at_activityStopped() {
    m_inactive = true;
    m_storedVisibility = m_visibility;

    setSliderVisible(false);
    setTreeVisible(false);

    m_treeShowButton->hide();
}

void QnWorkbenchUi::at_activityStarted() {
    m_inactive = false;

    setSliderVisible(m_storedVisibility.sliderVisible);
    setTreeVisible(m_storedVisibility.treeVisible);

    m_treeShowButton->show();
}


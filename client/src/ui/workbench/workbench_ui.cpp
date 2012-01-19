#include "workbench_ui.h"
#include <cassert>
#include <cmath> /* For std::floor. */

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>

#include <utils/common/event_signalizator.h>

#include <camera/resource_display.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/widget_opacity_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/ui_elements_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/items/image_button_widget.h>
#include <ui/graphics/items/resource_widget.h>

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

} // anonymous namespace

QnWorkbenchUi::QnWorkbenchUi(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_display(display),
    m_manager(display->instrumentManager())
{
    /* Install and configure instruments. */
    m_uiElementsInstrument = new UiElementsInstrument(this);
    m_manager->installInstrument(m_uiElementsInstrument, InstallationMode::INSTALL_BEFORE, m_display->paintForwardingInstrument());

    /* Create controls. */
    m_controlsWidget = m_uiElementsInstrument->widget();
    m_controlsWidget->setFlag(QGraphicsItem::ItemIsPanel);
    m_display->setLayer(m_controlsWidget, QnWorkbenchDisplay::UI_ELEMENTS_LAYER);

    QnSingleEventSignalizator *deactivationSignalizator = new QnSingleEventSignalizator(this);
    deactivationSignalizator->setEventType(QEvent::WindowDeactivate);
    m_controlsWidget->installEventFilter(deactivationSignalizator);

    connect(deactivationSignalizator,   SIGNAL(activated(QObject *, QEvent *)),                                                     this,                           SLOT(at_controlsWidget_deactivated()));
    connect(m_controlsWidget,               SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_controlsWidget_geometryChanged()));

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

    m_treeItem = new QGraphicsProxyWidget(m_controlsWidget);
    m_treeItem->setWidget(m_treeWidget);
    m_treeItem->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_treeItem->setFocusPolicy(Qt::StrongFocus);
    m_treeItem->setOpacity(normalTreeOpacity);

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
    m_treeBackgroundItem->stackBefore(m_treeItem);
    m_treeBackgroundItem->setOpacity(normalTreeBackgroundOpacity);

    m_treeBookmarkItem = new QnImageButtonWidget(m_controlsWidget);
    m_treeBookmarkItem->resize(15, 45);
    m_treeBookmarkItem->setOpacity(normalTreeBackgroundOpacity);
    m_treeBookmarkItem->setPixmap(QnImageButtonWidget::DEFAULT, Skin::pixmap("slide_right.png"));
    m_treeBookmarkItem->setPixmap(QnImageButtonWidget::HOVERED, Skin::pixmap("slide_right_hover.png"));
    m_treeBookmarkItem->setPixmap(QnImageButtonWidget::CHECKED, Skin::pixmap("slide_left.png"));
    m_treeBookmarkItem->setPixmap(QnImageButtonWidget::CHECKED | QnImageButtonWidget::HOVERED, Skin::pixmap("slide_left_hover.png"));
    m_treeBookmarkItem->setCheckable(true);
    m_treeBookmarkItem->setAnimationSpeed(4.0);

    m_treeOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeOpacityProcessor->addTargetItem(m_treeItem);
    m_treeOpacityProcessor->addTargetItem(m_treeBookmarkItem);

    m_treeHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeHidingProcessor->addTargetItem(m_treeItem);
    m_treeHidingProcessor->addTargetItem(m_treeBookmarkItem);
    m_treeHidingProcessor->setHoverLeaveDelay(1000);
    m_treeHidingProcessor->setFocusLeaveDelay(1000);

    m_treeShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeShowingProcessor->addTargetItem(m_treeBookmarkItem);
    m_treeShowingProcessor->setHoverEnterDelay(250);

    m_treePositionAnimator = new VariantAnimator(this);
    m_treePositionAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_treePositionAnimator->setTargetObject(m_treeItem);
    m_treePositionAnimator->setAccessor(new PropertyAccessor("pos"));
    m_treePositionAnimator->setSpeed(m_treeItem->size().width() * 2.0);
    m_treePositionAnimator->setTimeLimit(500);

    m_treeOpacityAnimatorGroup = new AnimatorGroup(this);
    m_treeOpacityAnimatorGroup->setTimer(display->animationInstrument()->animationTimer());
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeItem));
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeBookmarkItem));

    setTreeVisible(false, false);

    connect(m_treeBookmarkItem,         SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_treeBookmarkItem_toggled(bool)));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(at_treeOpacityProcessor_hoverLeft()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_treeOpacityProcessor_hoverEntered()));
    connect(m_treeHidingProcessor,      SIGNAL(hoverFocusLeft()),                                                                   this,                           SLOT(at_treeHidingProcessor_hoverFocusLeft()));
    connect(m_treeShowingProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_treeShowingProcessor_hoverEntered()));
    connect(m_treeItem,                 SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_treeItem_geometryChanged()));

    /* Navigation slider. */
    m_navigationItem = new NavigationItem(m_controlsWidget);
    m_navigationItem->setOpacity(normalSliderOpacity);

    m_sliderOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_sliderOpacityProcessor->addTargetItem(m_navigationItem);

    m_sliderPositionAnimator = new VariantAnimator(this);
    m_sliderPositionAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_sliderPositionAnimator->setTargetObject(m_navigationItem);
    m_sliderPositionAnimator->setAccessor(new PropertyAccessor("pos"));
    m_sliderPositionAnimator->setSpeed(m_navigationItem->size().height() * 2.0);
    m_sliderPositionAnimator->setTimeLimit(500);

    setSliderVisible(false, false);

    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_sliderOpacityProcessor_hoverEntered()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                                                                        this,                           SLOT(at_sliderOpacityProcessor_hoverLeft()));
    connect(m_navigationItem,           SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_navigationItem_geometryChanged()));
    connect(m_navigationItem,           SIGNAL(actualCameraChanged(CLVideoCamera *)),                                               this,                           SLOT(at_navigationItem_actualCameraChanged(CLVideoCamera *)));
    connect(m_navigationItem,           SIGNAL(playbackMaskChanged(const QnTimePeriodList &)),                                      m_display,                      SIGNAL(playbackMaskChanged(const QnTimePeriodList &)));
    connect(m_display,                  SIGNAL(displayingStateChanged(QnResourcePtr, bool)),                                        m_navigationItem,               SLOT(onDisplayingStateChanged(QnResourcePtr, bool)));

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
    m_treeVisible = visible;
    m_treeBookmarkItem->setChecked(visible);

    QPointF newPos = QPointF(visible ? 0.0 : -m_treeItem->size().width() - 1.0 /* Just in case */, 0.0);
    if (animate) {
        m_treePositionAnimator->animateTo(newPos);
    } else {
        m_treeItem->setPos(newPos);
    }
}

void QnWorkbenchUi::setSliderVisible(bool visible, bool animate) {
    m_sliderVisible = visible;

    QPointF newPos = QPointF(0.0, m_uiElementsInstrument->widget()->size().height() + (visible ? -m_navigationItem->size().height() : 32.0 /* So that tooltips are not visible. */));
    if (animate)
        m_sliderPositionAnimator->animateTo(newPos);
    else
        m_navigationItem->setPos(newPos);
}

void QnWorkbenchUi::toggleTreeVisible() {
    setTreeVisible(!m_treeVisible);
}

void QnWorkbenchUi::updateTreeGeometry() {
    QPointF sliderTargetPos;
    bool deferrable;
    if(m_sliderPositionAnimator->isRunning()) {
        sliderTargetPos = m_sliderPositionAnimator->targetValue().toPointF();
        QPointF sliderPos = m_navigationItem->pos();

        /* If animation is about to end, then geometry sync cannot be deferred. */
        deferrable = !qFuzzyCompare(sliderTargetPos, sliderPos);
    } else {
        sliderTargetPos = m_navigationItem->pos();
        deferrable = false;
    }

    qreal oldWidth = m_treeItem->size().width();
    qreal oldHeight = m_treeItem->size().height();
    qreal newWidth = oldWidth;
    qreal newHeight = qMin(sliderTargetPos.y(), m_controlsWidget->size().height()) - m_treeItem->pos().y();
    if(deferrable && newHeight < oldHeight)
        return;

    QRectF geometry = QRectF(
        m_treeItem->pos().x(),
        m_treeItem->pos().y(),
        newWidth,
        newHeight
    );

    if(qFuzzyCompare(geometry, m_treeItem->geometry()))
        return;

    m_treeItem->setGeometry(geometry);
}

void QnWorkbenchUi::updateViewportMargins() {
    m_display->setViewportMargins(QMargins(
        0, //std::floor(qMax(0.0, m_treeItem->pos().x() + m_treeItem->size().width())),
        0,
        0,
        std::floor(qMax(0.0, m_controlsWidget->size().height() - m_navigationItem->pos().y()))
    ));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchUi::at_display_widgetChanged(QnWorkbench::ItemRole role) {
    /* Update navigation item's target. */
    QnResourceWidget *widget = m_display->widget(QnWorkbench::ZOOMED);
    if(widget == NULL)
        widget = m_display->widget(QnWorkbench::RAISED);
    m_navigationItem->setVideoCamera(widget == NULL ? NULL : widget->display()->camera());
}

void QnWorkbenchUi::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
    if(cameraResource != NULL) {
        connect(widget, SIGNAL(motionRegionSelected(QnResourcePtr, QRegion)), m_navigationItem, SLOT(loadMotionPeriods(QnResourcePtr, QRegion)));
        connect(m_navigationItem, SIGNAL(clearMotionSelection()), widget, SLOT(clearMotionSelection()));
        m_navigationItem->addReserveCamera(widget->display()->camera());
    }
}

void QnWorkbenchUi::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
    if(cameraResource != NULL)
        m_navigationItem->removeReserveCamera(widget->display()->camera());
}

void QnWorkbenchUi::at_controlsWidget_deactivated() {
    /* Re-activate it. */
    display()->scene()->setActiveWindow(m_uiElementsInstrument->widget());
}

void QnWorkbenchUi::at_controlsWidget_geometryChanged() {
    QGraphicsWidget *controlsWidget = m_uiElementsInstrument->widget();
    QSizeF size = controlsWidget->size();
    if(qFuzzyCompare(m_controlsWidgetSize, size))
        return;
    QSizeF oldSize = m_controlsWidgetSize;
    m_controlsWidgetSize = size;

    /* We lay everything out manually. */

    m_navigationItem->setGeometry(QRectF(
        0.0,
        m_navigationItem->pos().y() - oldSize.height() + size.height(),
        size.width(),
        m_navigationItem->size().height()
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
    opacityAnimator(m_navigationItem)->animateTo(hoverSliderOpacity);
}

void QnWorkbenchUi::at_sliderOpacityProcessor_hoverLeft() {
    opacityAnimator(m_navigationItem)->animateTo(normalSliderOpacity);
}

void QnWorkbenchUi::at_treeItem_geometryChanged() {
    QRectF geometry = m_treeItem->geometry();

    bool visible = geometry.right() >= 0.0;
    m_treeItem->setVisible(visible);
    m_treeBackgroundItem->setVisible(visible);

    m_treeBackgroundItem->setGeometry(geometry);
    m_treeBookmarkItem->setPos(QPointF(
        geometry.right(),
        (geometry.top() + geometry.bottom() - m_treeBookmarkItem->size().height()) / 2
    ));

    updateViewportMargins();
}

void QnWorkbenchUi::at_treeHidingProcessor_hoverFocusLeft() {
    setTreeVisible(false);
}

void QnWorkbenchUi::at_treeShowingProcessor_hoverEntered() {
    setTreeVisible(true);
    m_treeHidingProcessor->forceHoverEnter();
    m_treeOpacityProcessor->forceHoverEnter();
}

void QnWorkbenchUi::at_treeOpacityProcessor_hoverLeft() {
    m_treeOpacityAnimatorGroup->pause();
    opacityAnimator(m_treeItem)->setTargetValue(normalTreeOpacity);
    opacityAnimator(m_treeBackgroundItem)->setTargetValue(normalTreeBackgroundOpacity);
    opacityAnimator(m_treeBookmarkItem)->setTargetValue(normalTreeBackgroundOpacity);
    m_treeOpacityAnimatorGroup->start();
}

void QnWorkbenchUi::at_treeOpacityProcessor_hoverEntered() {
    m_treeOpacityAnimatorGroup->pause();
    opacityAnimator(m_treeItem)->setTargetValue(hoverTreeOpacity);
    opacityAnimator(m_treeBackgroundItem)->setTargetValue(hoverTreeBackgroundOpacity);
    opacityAnimator(m_treeBookmarkItem)->setTargetValue(hoverTreeBackgroundOpacity);
    m_treeOpacityAnimatorGroup->start();
}

void QnWorkbenchUi::at_treeBookmarkItem_toggled(bool checked) {
    m_treeShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */
    setTreeVisible(checked);
}

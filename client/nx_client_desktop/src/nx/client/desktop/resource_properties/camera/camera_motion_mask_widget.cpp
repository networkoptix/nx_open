#include "camera_motion_mask_widget.h"

#include <limits>

#include <QtWidgets/QVBoxLayout>

#include <QtOpenGL/QGLWidget>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/animation/curtain_animator.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/click_instrument.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_controller.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/globals.h>
#include <nx/client/desktop/ui/workbench/layouts/layout_factory.h>

namespace nx {
namespace client {
namespace desktop {

CameraMotionMaskWidget::CameraMotionMaskWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_readOnly(false),
    m_controlMaxRects(false)
{
    init();
}

void CameraMotionMaskWidget::init()
{
    m_motionSensitivity = 0;

    /* Set up scene & view. */
    m_scene.reset(new QnGraphicsScene(this));
    m_view.reset(new QnGraphicsView(m_scene.data(), this));
    m_view->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_view->setLineWidth(1);
    m_view->setAutoFillBackground(true);

    /* Set up model & control machinery. */
    m_context.reset(new QnWorkbenchContext(context()->accessController(), this));

    createWorkbenchLayout();

    auto display = m_context->display();
    display->setLightMode(Qn::LightModeFull);
    display->setScene(m_scene.data());
    display->setView(m_view.data());
    display->curtainAnimator()->setCurtainItem(nullptr);
    m_controller.reset(new QnWorkbenchController(display));

    /* Disable unused instruments. */
    m_controller->setMenuEnabled(false);

    /* We need to listen to viewport resize events to make sure that our widget is always positioned at viewport's center. */
    SignalingInstrument *resizeSignalingInstrument = new SignalingInstrument(Instrument::Viewport, Instrument::makeSet(QEvent::Resize), this);
    display->instrumentManager()->installInstrument(resizeSignalingInstrument);
    connect(resizeSignalingInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(at_viewport_resized()));

    /* Create motion mask selection instrument. */
    m_motionSelectionInstrument = dynamic_cast<MotionSelectionInstrument*>(
        m_controller->motionSelectionInstrument());
    m_motionSelectionInstrument->setSelectionModifiers(Qt::NoModifier);
    m_motionSelectionInstrument->setMultiSelectionModifiers(Qt::NoModifier);
    m_motionSelectionInstrument->setBrush(qnGlobals->motionMaskRubberBandColor());
    m_motionSelectionInstrument->setPen(qnGlobals->motionMaskRubberBandBorderColor());
    disconnect(m_motionSelectionInstrument, NULL,                                               m_controller,   NULL); // TODO: #Elric controller flags?
    connect(m_motionSelectionInstrument,    &MotionSelectionInstrument::motionRegionSelected,   this,           &CameraMotionMaskWidget::at_motionRegionSelected);
    connect(m_motionSelectionInstrument,    &MotionSelectionInstrument::motionRegionCleared,    this,           &CameraMotionMaskWidget::clearMotion);

    /* Create motion region floodfill instrument. */
    m_clickInstrument = new ClickInstrument(Qt::LeftButton, 0, Instrument::Item, this);
    connect(m_clickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),   this,   SLOT(at_itemClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    display->instrumentManager()->installInstrument(m_clickInstrument);

    /* Set up UI. */
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view.data());
    setLayout(layout);
}

void CameraMotionMaskWidget::createWorkbenchLayout()
{
    const auto workbenchLayout = qnWorkbenchLayoutsFactory->create(this);
    workbenchLayout->setCellSpacing(0);
    workbenchLayout->setFlags(workbenchLayout->flags()
        | QnLayoutFlag::FixedViewport
        | QnLayoutFlag::MotionWidget);
    m_context->workbench()->setCurrentLayout(workbenchLayout);
}

CameraMotionMaskWidget::~CameraMotionMaskWidget() {
    return;
}

bool CameraMotionMaskWidget::isReadOnly() const {
    return m_readOnly;
}

void CameraMotionMaskWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    if(readOnly) {
        m_motionSelectionInstrument->disable();
        m_clickInstrument->disable();

    } else {
        m_motionSelectionInstrument->enable();
        m_clickInstrument->enable();
    }

    m_readOnly = readOnly;
}

QList<QnMotionRegion> CameraMotionMaskWidget::motionRegionList() const {
    if (m_resourceWidget)
        return m_resourceWidget->motionSensitivity();
    else
        return QList<QnMotionRegion>();
}

QnResourcePtr CameraMotionMaskWidget::camera() const {
    return m_camera;
}

void CameraMotionMaskWidget::setCamera(const QnResourcePtr& resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (m_camera == camera)
        return;

    m_camera = camera;

    if (m_camera && m_context->workbench()->layouts().empty())
        createWorkbenchLayout();

    m_context->workbench()->currentLayout()->clear();
    m_resourceWidget = nullptr;

    if (m_camera)
    {
        /* Add single item to the layout. */
        QnWorkbenchItem* item = new QnWorkbenchItem(resource, QnUuid::createUuid(), this);
        item->setPinned(true);
        item->setGeometry(QRect(0, 0, 1, 1));
        item->setRotation(0);

        QnResourceWidget::Options forcedOptions =
            QnResourceWidget::DisplayMotionSensitivity |
            QnResourceWidget::DisplayMotion |
            QnResourceWidget::WindowRotationForbidden |
            QnResourceWidget::SyncPlayForbidden |
            QnResourceWidget::InfoOverlaysForbidden;

        item->setData(Qn::ItemWidgetOptions, forcedOptions);

        m_context->workbench()->currentLayout()->addItem(item);
        m_context->workbench()->setItem(Qn::ZoomedRole, item);

        /* Set up the corresponding widget. */
        m_resourceWidget = dynamic_cast<QnMediaResourceWidget *>(m_context->display()->widget(item)); // TODO: #Elric check for NULL
        NX_ASSERT(m_resourceWidget);

        if (m_resourceWidget)
        {
            qApp->sendPostedEvents(m_resourceWidget, QnEvent::Customize);
            m_resourceWidget->setFrameOpacity(0.0);
        }
    }

    m_motionSensitivity = 0;

    /* Consider motion mask list changed. */
    emit motionRegionListChanged();
}

void CameraMotionMaskWidget::showTooManyWindowsMessage(const QnMotionRegion& region,
    const QnMotionRegion::ErrorCode errCode)
{
    switch (errCode)
    {
        case QnMotionRegion::ErrorCode::Windows:
            QnMessageBox::warning(this,
                tr("Too many motion windows"),
                tr("Maximum number of motion windows for the current camera is %1,"
                    " and %2 motion windows are currently selected.")
                .arg(m_camera->motionWindowCount()).arg(region.getMotionRectCount()));
            break;
        case QnMotionRegion::ErrorCode::Sens:
        {
            // Handle case when user can set different settings for each sensor of the camera.
            const bool isPanoramicCamera = m_camera->getVideoLayout()->channelCount() > 1;
            const bool isHardwareMotion = m_camera->getMotionType() == Qn::MT_HardwareGrid;

            const QString message = isPanoramicCamera && isHardwareMotion
                ? tr("Maximum number of motion sensitivity settings for any sensor of the current"
                    " camera is %1, and %2 settings are currently selected.")
                : tr("Maximum number of motion sensitivity settings for the current camera is %1,"
                    " and %2 settings are currently selected.");

            QnMessageBox::warning(
                this,
                tr("Too many motion sensitivity settings"),
                message.arg(m_camera->motionSensWindowCount()).arg(region.getMotionSensCount()));
            break;
        }
        case QnMotionRegion::ErrorCode::Masks:
            QnMessageBox::warning(this,
                tr("Too many motion areas"),
                tr("Maximum number of motion areas for the current camera is %1,"
                    " and %2 motion areas are currently selected.").arg(
                        m_camera->motionMaskWindowCount()).arg(region.getMaskRectCount()));
            break;
        default:
            break;
    }
}

void CameraMotionMaskWidget::setControlMaxRects(bool controlMaxRects) {
    m_controlMaxRects = controlMaxRects;
};

bool CameraMotionMaskWidget::isControlMaxRects() const {
    return m_controlMaxRects;
}

int CameraMotionMaskWidget::motionSensitivity() const {
    return m_motionSensitivity;
}

void CameraMotionMaskWidget::setMotionSensitivity(int motionSensitivity) {
    m_motionSensitivity = motionSensitivity;
}

void CameraMotionMaskWidget::clearMotion() {
    if (!m_resourceWidget)
        return;
    bool changed = false;

    const QList<QnMotionRegion> &regions = m_resourceWidget->motionSensitivity();
    for (int i = 0; i < regions.size(); ++i) {
        if(!regions[i].isEmpty()) {
            changed = true;
        }
    }
    m_resourceWidget->clearMotionSensitivity();

    if(changed)
        emit motionRegionListChanged();
}

bool CameraMotionMaskWidget::isValidMotionRegion() {
    if (m_resourceWidget && m_controlMaxRects) {
        QnConstResourceVideoLayoutPtr layout = m_camera->getVideoLayout();
        const QList<QnMotionRegion> &regions = m_resourceWidget->motionSensitivity();
        for (int i = 0; i < qMin(regions.size(), layout->channelCount()); ++i) {
            QnMotionRegion::ErrorCode errCode = regions[i].isValid(m_camera->motionWindowCount(),
                m_camera->motionMaskWindowCount(), m_camera->motionSensWindowCount());
            if (errCode != QnMotionRegion::ErrorCode::Ok) {
                showTooManyWindowsMessage(regions[i], errCode);
                return false;
            }
        }
    }
    return true;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void CameraMotionMaskWidget::at_viewport_resized() {
    m_context->display()->fitInView(false);
}

void CameraMotionMaskWidget::at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *widget, const QRect &gridRect) {
    if (!m_resourceWidget)
        return;

    bool changed = widget->addToMotionSensitivity(gridRect, m_motionSensitivity);
    if(changed)
        emit motionRegionListChanged();
}

void CameraMotionMaskWidget::at_itemClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    if (!m_resourceWidget)
        return;

    QPointF pos = info.scenePos() - item->pos();
    QPoint gridPos = m_resourceWidget->mapToMotionGrid(pos);
    if (m_resourceWidget->setMotionSensitivityFilled(gridPos, m_motionSensitivity))
        emit motionRegionListChanged();
}

QVector<QColor> CameraMotionMaskWidget::motionSensitivityColors() const
{
    return m_resourceWidget ? m_resourceWidget->motionSensitivityColors() : QVector<QColor>();
}

} // namespace desktop
} // namespace client
} // namespace nx

#include "camera_motion_mask_widget.h"
#include <QVBoxLayout>
#include <QGLWidget>
#include "utils/common/checked_cast.h"
#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/instruments/signaling_instrument.h"
#include "ui/graphics/instruments/instrument_manager.h"
#include "ui/graphics/instruments/motion_selection_instrument.h"
#include "ui/graphics/instruments/click_instrument.h"
#include "ui/graphics/instruments/move_instrument.h"
#include "ui/graphics/instruments/resizing_instrument.h"
#include "ui/graphics/instruments/forwarding_instrument.h"
#include "ui/graphics/instruments/rubber_band_instrument.h"
#include "ui/graphics/items/resource_widget.h"
#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/style/globals.h"
#include "ui/workbench/workbench_context.h"

namespace {
    QList<QRegion> emptyMotionMaskList() {
        QList<QRegion> result;

        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
            result.push_back(QRegion());

        return result;
    }

} // anonymous namespace


QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(QWidget *parent): 
    QWidget(parent),
    m_readOnly(false)
{
	init();
}

void QnCameraMotionMaskWidget::init()
{
    m_motionMaskList = emptyMotionMaskList();

    /* Set up scene & view. */
    m_scene.reset(new QGraphicsScene(this));
    m_view.reset(new QnGraphicsView(m_scene.data(), this));
    m_view->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_view->setLineWidth(1);
    m_view->setAutoFillBackground(true);
    {
        /* Adjust palette so that inherited background painting is not needed. */
        QPalette palette = m_view->palette();
        palette.setColor(QPalette::Background, Qt::black);
        palette.setColor(QPalette::Base, Qt::black);
        m_view->setPalette(palette);
    }

    /* Set up model & control machinery. */
    m_context.reset(new QnWorkbenchContext(NULL, this));

    m_display.reset(new QnWorkbenchDisplay(m_context.data()));
    m_display->setScene(m_scene.data());
    m_display->setView(m_view.data());

    m_controller.reset(new QnWorkbenchController(m_display.data(), this));

    /* Disable unused instruments. */
    m_controller->motionSelectionInstrument()->disable();
    m_controller->itemRightClickInstrument()->disable();
    m_controller->moveInstrument()->setEffective(false);
    m_controller->resizingInstrument()->setEffective(false);
    m_controller->rubberBandInstrument()->disable();
    m_controller->itemLeftClickInstrument()->disable();

    /* We need to listen to viewport resize events to make sure that our widget is always positioned at viewport's center. */
    SignalingInstrument *resizeSignalingInstrument = new SignalingInstrument(Instrument::Viewport, Instrument::makeSet(QEvent::Resize), this);
    m_display->instrumentManager()->installInstrument(resizeSignalingInstrument);
    connect(resizeSignalingInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(at_viewport_resized()));

    /* Create motion mask selection instrument. */
	m_motionSelectionInstrument = new MotionSelectionInstrument(this);
    m_motionSelectionInstrument->setSelectionModifiers(Qt::NoModifier);
	m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Base, qnGlobals->motionMaskRubberBandColor());
	m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Border, qnGlobals->motionMaskRubberBandBorderColor());
    m_display->instrumentManager()->installInstrument(m_motionSelectionInstrument);

    ForwardingInstrument *itemMouseForwardingInstrument = m_controller->itemMouseForwardingInstrument();
	connect(m_motionSelectionInstrument,  SIGNAL(motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)),         this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)));
	connect(m_motionSelectionInstrument,  SIGNAL(motionRegionCleared(QGraphicsView *, QnResourceWidget *)),                         this,                           SLOT(at_motionRegionCleared(QGraphicsView *, QnResourceWidget *)));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    itemMouseForwardingInstrument,  SLOT(recursiveEnable()));

    /* Set up UI. */
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view.data());
    setLayout(layout);
}

QnCameraMotionMaskWidget::~QnCameraMotionMaskWidget()
{
}

bool QnCameraMotionMaskWidget::isReadOnly() const 
{
    return m_readOnly;
}

void QnCameraMotionMaskWidget::setReadOnly(bool readOnly) 
{
    if(m_readOnly == readOnly)
        return;

    if(readOnly) {
        m_motionSelectionInstrument->disable();
    } else {
        m_motionSelectionInstrument->enable();
    }

    m_readOnly = readOnly;
}

const QList<QRegion>& QnCameraMotionMaskWidget::motionMaskList() const
{
    return m_motionMaskList;
}

const QnResourcePtr &QnCameraMotionMaskWidget::camera() const {
    return m_camera; // TODO: returning temporary here.
}

void QnCameraMotionMaskWidget::setCamera(const QnResourcePtr& resource)
{
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(m_camera == camera)
        return;

	m_camera = camera;

    m_context->workbench()->currentLayout()->clear();

    if(!m_camera) {
        m_motionMaskList = emptyMotionMaskList();
    } else {
        m_motionMaskList = m_camera->getMotionMaskList();

        /* Add single item to the layout. */
        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid(), this);
        item->setPinned(true);
        item->setGeometry(QRect(0, 0, 1, 1));
        m_context->workbench()->currentLayout()->addItem(item);
        m_context->workbench()->setItem(Qn::ZoomedRole, item);

        /* Set up the corresponding widget. */
        QnResourceWidget *widget = m_display->widget(item);
        widget->setDisplayFlag(QnResourceWidget::DisplayButtons, false);
        widget->setDisplayFlag(QnResourceWidget::DisplayMotionGrid, true);
    }

    /* Consider motion mask list changed. */
    emit motionMaskListChanged();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraMotionMaskWidget::at_viewport_resized() {
    m_display->fitInView(false);
}

void QnCameraMotionMaskWidget::at_motionRegionSelected(QGraphicsView *, QnResourceWidget *widget, const QRect &gridRect)
{
    const QnVideoResourceLayout* layout = m_camera->getVideoLayout();
    bool changed = false;

    int numChannels = layout->numberOfChannels();
    for (int i = 0; i < numChannels; ++i)
    {
        QRect r(0, 0, MD_WIDTH, MD_HEIGHT);
        r.translate(layout->h_position(i)*MD_WIDTH, layout->v_position(i)*MD_HEIGHT);
        r = gridRect.intersected(r);
        r.translate(-layout->h_position(i)*MD_WIDTH, -layout->v_position(i)*MD_HEIGHT);

        if (!r.isEmpty()) 
        {
            QRegion oldRegion = m_motionMaskList[i];
            m_motionMaskList[i] += r;
            if(oldRegion != m_motionMaskList[i]) { /* Note: we cannot use QRegion::contains here as it checks for overlap, not for containment. */
                widget->addToMotionMask(r, i);
                changed = true;
            }
        }
    }

    if(changed)
        emit motionMaskListChanged();
}

void QnCameraMotionMaskWidget::at_motionRegionCleared(QGraphicsView *, QnResourceWidget *widget)
{
    bool changed = false;

    widget->clearMotionMask();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        if(!m_motionMaskList[i].isEmpty()) {
	        m_motionMaskList[i] = QRegion();
            changed = true;
        }
    }

    if(changed)
        emit motionMaskListChanged();
}

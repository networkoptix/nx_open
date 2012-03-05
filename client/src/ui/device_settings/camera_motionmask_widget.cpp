#include "camera_motionmask_widget.h"
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

QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(QWidget *parent)
	: QWidget(parent)
{
	init();
}

QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(const QnResourcePtr &resource, QWidget *parent)
	: QWidget(parent)
{
	init();
	setCamera(resource);
}

void QnCameraMotionMaskWidget::init()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        m_motionMaskList << QRegion();

    /* Set up scene & view. */
    m_scene = new QGraphicsScene(this);
    m_view = new QnGraphicsView(m_scene, this);
    m_view->setPaintFlags(QnGraphicsView::BACKGROUND_DONT_INVOKE_BASE | QnGraphicsView::FOREGROUND_DONT_INVOKE_BASE);
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
    m_context = new QnWorkbenchContext(NULL, this);

    m_display = new QnWorkbenchDisplay(m_context, this);
    m_display->setScene(m_scene);
    m_display->setView(m_view);
    m_display->setMarginFlags(0);

    m_controller = new QnWorkbenchController(m_display, this);

    /* Disable unused instruments. */
    m_controller->motionSelectionInstrument()->disable();
    m_controller->itemRightClickInstrument()->disable();
    m_controller->moveInstrument()->setEffective(false);
    m_controller->resizingInstrument()->setEffective(false);
    m_controller->rubberBandInstrument()->disable();

    /* We need to listen to viewport resize events to make sure that our widget is always positioned at viewport's center. */
    SignalingInstrument *resizeSignalingInstrument = new SignalingInstrument(Instrument::VIEWPORT, Instrument::makeSet(QEvent::Resize), this);
    m_display->instrumentManager()->installInstrument(resizeSignalingInstrument);
    connect(resizeSignalingInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(at_viewport_resized()));

    /* Create motion mask selection instrument. */
	MotionSelectionInstrument *motionSelectionInstrument = new MotionSelectionInstrument(this);
    motionSelectionInstrument->setSelectionModifiers(Qt::NoModifier);
	motionSelectionInstrument->setColor(MotionSelectionInstrument::Base, qnGlobals->motionMaskRubberBandColor());
	motionSelectionInstrument->setColor(MotionSelectionInstrument::Border, qnGlobals->motionMaskRubberBandBorderColor());
    m_display->instrumentManager()->installInstrument(motionSelectionInstrument);

    ForwardingInstrument *itemMouseForwardingInstrument = m_controller->itemMouseForwardingInstrument();
	connect(motionSelectionInstrument,  SIGNAL(motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)),         this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)));
	connect(motionSelectionInstrument,  SIGNAL(motionRegionCleared(QGraphicsView *, QnResourceWidget *)),                         this,                           SLOT(at_motionRegionCleared(QGraphicsView *, QnResourceWidget *)));
    connect(motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    itemMouseForwardingInstrument,  SLOT(recursiveEnable()));

    /* Set up UI. */
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view);
    setLayout(layout);
}

QnCameraMotionMaskWidget::~QnCameraMotionMaskWidget()
{
}

void QnCameraMotionMaskWidget::at_viewport_resized() {
    m_display->fitInView(false);
}

void QnCameraMotionMaskWidget::setCamera(const QnResourcePtr& resource)
{
	m_camera = resource.dynamicCast<QnVirtualCameraResource>();
	m_motionMaskList = m_camera->getMotionMaskList();

    /* Add single item to the layout. */
	QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid(), this);
	item->setPinned(true);
	item->setGeometry(QRect(0, 0, 1, 1));
	m_context->workbench()->currentLayout()->addItem(item);
	m_context->workbench()->setItem(QnWorkbench::ZOOMED, item);

    /* Set up the corresponding widget. */
    m_widget = m_display->widget(item);
    widget()->setDisplayFlag(QnResourceWidget::DISPLAY_BUTTONS, false);
    widget()->setDisplayFlag(QnResourceWidget::DISPLAY_MOTION_GRID, true);
}

void QnCameraMotionMaskWidget::at_motionRegionSelected(QGraphicsView *view, QnResourceWidget *widget, const QRect &gridRect)
{
    const QnVideoResourceLayout* layout = m_camera->getVideoLayout();

    int numChannels = layout->numberOfChannels();
    for (int i = 0; i < numChannels; ++i)
    {
        QRect r(0, 0, MD_WIDTH, MD_HEIGHT);
        r.translate(layout->h_position(i)*MD_WIDTH, layout->v_position(i)*MD_HEIGHT);
        r = gridRect.intersected(r);
        r.translate(-layout->h_position(i)*MD_WIDTH, -layout->v_position(i)*MD_HEIGHT);

        if (!r.isEmpty()) 
        {
            widget->addToMotionMask(r, i);
            m_motionMaskList[i] += r;
        }
    }
}

void QnCameraMotionMaskWidget::at_motionRegionCleared(QGraphicsView *view, QnResourceWidget *widget)
{
    widget->clearMotionMask();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
	    m_motionMaskList[i] = QRegion();
}

const QList<QRegion>& QnCameraMotionMaskWidget::motionMaskList() const
{
	return m_motionMaskList;
}

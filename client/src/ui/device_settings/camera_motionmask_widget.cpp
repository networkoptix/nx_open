#include "camera_motionmask_widget.h"
#include <QVBoxLayout>
#include <QGLWidget>
#include "utils/common/checked_cast.h"
#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/instruments/signaling_instrument.h"
#include "ui/graphics/instruments/instrument_manager.h"
#include "ui/graphics/instruments/motion_selection_instrument.h"
#include "ui/graphics/items/resource_widget.h"
#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/style/globals.h"

QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(QWidget *parent)
	: QWidget(parent),
	  m_item(0)
{
	init();
}

QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(const QnResourcePtr &resource, QWidget *parent)
	: QWidget(parent),
	  m_item(0)
{
	init();
	setCamera(resource);
}

void QnCameraMotionMaskWidget::init()
{
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
    const QSizeF defaultCellSize = QSizeF(15000.0, 10000.0); /* Graphics scene has problems with handling mouse events on small scales, so the larger these numbers, the better. */
    const QSizeF defaultSpacing = QSizeF(2500.0, 2500.0);
    
    m_workbench = new QnWorkbench(this);
    m_workbench->mapper()->setCellSize(defaultCellSize);
    m_workbench->mapper()->setSpacing(defaultSpacing);

    m_display = new QnWorkbenchDisplay(m_workbench, this);
    m_display->setScene(m_scene);
    m_display->setView(m_view);
    m_display->setMarginFlags(0);

    m_controller = new QnWorkbenchController(m_display, this);

    /* We need to listen to viewport resize events to make sure that our widget is always positioned at viewport's center. */
    SignalingInstrument *resizeSignalingInstrument = new SignalingInstrument(Instrument::VIEWPORT, Instrument::makeSet(QEvent::Resize), this);
    m_display->instrumentManager()->installInstrument(resizeSignalingInstrument);
    connect(resizeSignalingInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(at_viewport_resized()));

	MotionSelectionInstrument* motionSelectionInstrument = m_controller->motionSelectionInstrument();
	motionSelectionInstrument->setSelectionModifiers(Qt::NoModifier);

	motionSelectionInstrument->setColor(MotionSelectionInstrument::Base, Globals::motionMaskSelectionColor());
	// motionSelectionInstrument->setColor(MotionSelectionInstrument::Border);

	connect(motionSelectionInstrument,  SIGNAL(motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)),         this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)));
	connect(motionSelectionInstrument,  SIGNAL(motionRegionCleared(QGraphicsView *, QnResourceWidget *)),                         this,                           SLOT(at_motionRegionCleared(QGraphicsView *, QnResourceWidget *)));

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
	m_motionMask = m_camera->getMotionMask();

    /* Add single item to the layout. */
	m_item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid(), this);
	m_item->setPinned(true);
	m_item->setGeometry(QRect(0, 0, 1, 1));
	m_workbench->currentLayout()->addItem(m_item);
	m_workbench->setItem(QnWorkbench::ZOOMED, m_item);
}

void QnCameraMotionMaskWidget::displayMotionGrid(bool display)
{
	if (m_scene->items().isEmpty())
		return;

	QnResourceWidget *widget = m_display->widget(m_item); // qobject_cast<QnResourceWidget *>(m_scene->items().first()->toGraphicsObject());
	if(!widget)
		return;

	widget->setDisplayFlag(QnResourceWidget::DISPLAY_MOTION_GRID, display);
}

void QnCameraMotionMaskWidget::at_motionRegionSelected(QGraphicsView *view, QnResourceWidget *widget, const QRect &rect)
{
	m_motionMask += rect;
}

void QnCameraMotionMaskWidget::at_motionRegionCleared(QGraphicsView *view, QnResourceWidget *rects)
{
	m_motionMask = QRegion();
}

const QRegion & QnCameraMotionMaskWidget::motionMask() const
{
	return m_motionMask;
}

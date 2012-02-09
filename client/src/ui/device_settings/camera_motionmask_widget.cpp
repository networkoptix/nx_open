#include "camera_motionmask_widget.h"
#include <QVBoxLayout>
#include <QGLWidget>
#include "utils/common/checked_cast.h"
#include "ui/graphics/view/graphics_view.h"
#include "ui/graphics/instruments/signaling_instrument.h"
#include "ui/graphics/instruments/instrument_manager.h"
#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_layout.h"

QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(const QnResourcePtr &resource, QWidget *parent)
    : QWidget(parent)
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

    /* Set up UI. */
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view);
    setLayout(layout);

    /* Add single item to the layout. */
    QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid(), this);
    item->setPinned(true);
    item->setGeometry(QRect(0, 0, 1, 1));
    m_workbench->currentLayout()->addItem(item);
    m_workbench->setItem(QnWorkbench::ZOOMED, item);
}

QnCameraMotionMaskWidget::~QnCameraMotionMaskWidget() {}

void QnCameraMotionMaskWidget::at_viewport_resized() {
    m_display->fitInView(false);
}
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
    QList<QnMotionRegion> emptyMotionRegionList() {
        QList<QnMotionRegion> result;

        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
            result.push_back(QnMotionRegion());

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
    m_motionSensitivity = 0;
    //m_motionRegionList = emptyMotionRegionList();

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
    m_motionSelectionInstrument->setMultiSelectionModifiers(Qt::NoModifier);
	m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Base, qnGlobals->motionMaskRubberBandColor());
	m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Border, qnGlobals->motionMaskRubberBandBorderColor());
    m_display->instrumentManager()->installInstrument(m_motionSelectionInstrument);

    m_clickInstrument = new ClickInstrument(Qt::LeftButton, 0, Instrument::ITEM, this);
    connect(m_clickInstrument,  SIGNAL(clicked(QGraphicsView*, QGraphicsItem*, const ClickInfo&)),                                  this,                           SLOT(at_itemClicked(QGraphicsView*, QGraphicsItem*, const ClickInfo&)));
    m_display->instrumentManager()->installInstrument(m_clickInstrument);

    ForwardingInstrument *itemMouseForwardingInstrument = m_controller->itemMouseForwardingInstrument();
	connect(m_motionSelectionInstrument,  SIGNAL(motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)),         this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &)));
	connect(m_motionSelectionInstrument,  SIGNAL(motionRegionCleared(QGraphicsView *, QnResourceWidget *)),                         this,                           SLOT(at_motionRegionCleared()));
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

const QList<QnMotionRegion>& QnCameraMotionMaskWidget::motionRegionList() const
{
    if (m_resourceWidget)
        return m_resourceWidget->getMotionRegionList();
    else
        return QList<QnMotionRegion>();
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
        //m_motionRegionList = emptyMotionRegionList();
        m_resourceWidget = 0;
    } else {
        /* Add single item to the layout. */
        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid(), this);
        item->setPinned(true);
        item->setGeometry(QRect(0, 0, 1, 1));
        m_context->workbench()->currentLayout()->addItem(item);
        m_context->workbench()->setItem(Qn::ZoomedRole, item);

        /* Set up the corresponding widget. */
        m_resourceWidget = m_display->widget(item);
        m_resourceWidget->setDrawMotionWindows(QnResourceWidget::DrawAllMotionInfo);
        m_resourceWidget->setDisplayFlag(QnResourceWidget::DISPLAY_BUTTONS, false);
        m_resourceWidget->setDisplayFlag(QnResourceWidget::DISPLAY_MOTION_GRID, true);
        //m_resourceWidget->setMotionRegionList(m_camera->getMotionRegionList());
    }

    /* Consider motion mask list changed. */
    emit motionRegionListChanged();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnCameraMotionMaskWidget::at_viewport_resized() {
    m_display->fitInView(false);
}

void QnCameraMotionMaskWidget::at_motionRegionSelected(QGraphicsView *, QnResourceWidget *widget, const QRect &gridRect)
{
    if (!m_resourceWidget)
        return;

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
            if (widget->addToMotionRegion(m_motionSensitivity, r, i)) {
                changed = true;
            }
            else {
                showToManyWindowsMessage();
            }
        }
    }
    
    if(changed)
        emit motionRegionListChanged();
}


void QnCameraMotionMaskWidget::at_motionRegionCleared()
{
    if (!m_resourceWidget)
        return;
    bool changed = false;

    QList<QnMotionRegion>& regions = m_resourceWidget->getMotionRegionList();
    for (int i = 0; i < regions.size(); ++i) {
        if(!regions[i].isEmpty()) {
            changed = true;
        }
    }
    m_resourceWidget->clearMotionRegions();

    if(changed)
        emit motionRegionListChanged();
}

void QnCameraMotionMaskWidget::setMaxMotionRects(int value)
{
    bool needShowError = false;
    if (m_resourceWidget) {
        QList<QnMotionRegion>& regions = m_resourceWidget->getMotionRegionList();
        for (int i = 0; i < regions.size(); ++i) {
            regions[i].setMaxRectCount(value);
            if (!regions[i].isValid())
                needShowError = true;
        }
    }
    if (needShowError)
        showToManyWindowsMessage();
}

void QnCameraMotionMaskWidget::setMotionSensitivity(int value)
{
    m_motionSensitivity = value;
}

void QnCameraMotionMaskWidget::clearMotion()
{
    at_motionRegionCleared();
}

int QnCameraMotionMaskWidget::gridPosToChannelPos(QPoint& pos)
{
    const QnVideoResourceLayout* layout = m_camera->getVideoLayout();
    for (int i = 0; i < layout->numberOfChannels(); ++i)
    {
        QRect r(layout->h_position(i)*MD_WIDTH, layout->v_position(i)*MD_HEIGHT, MD_WIDTH, MD_HEIGHT);
        if (r.contains(pos)) 
        {
            pos -= r.topLeft();
            return i;
        }
    }
    return 0;
}

void QnCameraMotionMaskWidget::at_itemClicked(QGraphicsView* view, QGraphicsItem* item, const ClickInfo& info)
{
    if (!m_resourceWidget)
        return;

    QPointF pos = info.scenePos() - item->pos();
    QPoint gridPos = m_resourceWidget->mapToMotionGrid(pos);
    int channel = gridPosToChannelPos(gridPos);
    if (m_resourceWidget->getMotionRegionList()[channel].updateSensitivityAt(gridPos, m_motionSensitivity))
        emit motionRegionListChanged();
}

void QnCameraMotionMaskWidget::showToManyWindowsMessage()
{
    QList<QnMotionRegion>& regions = m_resourceWidget->getMotionRegionList();
    int maxCnt = 0;
    for (int i = 0; i < regions.size(); ++i)
        maxCnt = qMax(maxCnt, regions[i].getCurrentRectCount());
    QMessageBox::warning(this, tr("Too many motion windows"), tr("Maximum amount of motion windows for current camera is %1. Now selected %2 motion windows").arg(m_camera->motionWindowCnt()).arg(maxCnt));
}

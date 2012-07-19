#include "camera_motion_mask_widget.h"

#include <limits>

#include <QtGui/QVBoxLayout>
#include <QtGui/QMessageBox>

#include <QtOpenGL/QGLWidget>

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
#include "ui/graphics/items/media_resource_widget.h"
#include "ui/workbench/workbench_item.h"
#include "ui/workbench/workbench.h"
#include "ui/workbench/workbench_grid_mapper.h"
#include "ui/workbench/workbench_display.h"
#include "ui/workbench/workbench_controller.h"
#include "ui/workbench/workbench_layout.h"
#include "ui/workbench/workbench_context.h"
#include "ui/style/globals.h"


QnCameraMotionMaskWidget::QnCameraMotionMaskWidget(QWidget *parent): 
    QWidget(parent),
    m_readOnly(false),
    m_needControlMaxRects(false)
{
	init();
}

void QnCameraMotionMaskWidget::init() {
    m_motionSensitivity = 0;

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
    
    QnWorkbenchDisplay *display = m_context->display();
    display->setScene(m_scene.data());
    display->setView(m_view.data());
    m_controller.reset(new QnWorkbenchController(display));

    /* Disable unused instruments. */
    m_controller->motionSelectionInstrument()->disable();
    m_controller->itemRightClickInstrument()->disable();
    m_controller->moveInstrument()->disable();
    m_controller->resizingInstrument()->disable();
    m_controller->rubberBandInstrument()->disable();
    m_controller->itemLeftClickInstrument()->disable();

    /* We need to listen to viewport resize events to make sure that our widget is always positioned at viewport's center. */
    SignalingInstrument *resizeSignalingInstrument = new SignalingInstrument(Instrument::Viewport, Instrument::makeSet(QEvent::Resize), this);
    display->instrumentManager()->installInstrument(resizeSignalingInstrument);
    connect(resizeSignalingInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(at_viewport_resized()));

    /* Create motion mask selection instrument. */
	m_motionSelectionInstrument = new MotionSelectionInstrument(this);
    m_motionSelectionInstrument->setSelectionModifiers(Qt::NoModifier);
    m_motionSelectionInstrument->setMultiSelectionModifiers(Qt::NoModifier);
	m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Base, qnGlobals->motionMaskRubberBandColor());
	m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Border, qnGlobals->motionMaskRubberBandBorderColor());
    m_motionSelectionInstrument->setColor(MotionSelectionInstrument::MouseBorder, qnGlobals->motionMaskMouseFrameColor());
    display->instrumentManager()->installInstrument(m_motionSelectionInstrument);

    m_clickInstrument = new ClickInstrument(Qt::LeftButton, 0, Instrument::Item, this);
    connect(m_clickInstrument,  SIGNAL(clicked(QGraphicsView*, QGraphicsItem*, const ClickInfo&)),                                  this,                           SLOT(at_itemClicked(QGraphicsView*, QGraphicsItem*, const ClickInfo&)));
    display->instrumentManager()->installInstrument(m_clickInstrument);

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

QnCameraMotionMaskWidget::~QnCameraMotionMaskWidget() {
    return;
}

bool QnCameraMotionMaskWidget::isReadOnly() const {
    return m_readOnly;
}

void QnCameraMotionMaskWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    if(readOnly) {
        m_motionSelectionInstrument->disable();
    } else {
        m_motionSelectionInstrument->enable();
    }

    m_readOnly = readOnly;
}

const QList<QnMotionRegion> &QnCameraMotionMaskWidget::motionRegionList() const {
    if (m_resourceWidget)
        return m_resourceWidget->motionSensitivity();
    else
        return QList<QnMotionRegion>();
}

const QnResourcePtr &QnCameraMotionMaskWidget::camera() const {
    return m_camera; // TODO: returning temporary here.
}

void QnCameraMotionMaskWidget::setCamera(const QnResourcePtr& resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(m_camera == camera)
        return;

	m_camera = camera;

    m_context->workbench()->currentLayout()->clear();

    if(!m_camera) {
        m_resourceWidget = 0;
        m_motionSensitivity = QnMotionRegion::MIN_SENSITIVITY;
    } else {
        /* Add single item to the layout. */
        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid(), this);
        item->setPinned(true);
        item->setGeometry(QRect(0, 0, 1, 1));
        m_context->workbench()->currentLayout()->addItem(item);
        m_context->workbench()->setItem(Qn::ZoomedRole, item);

        /* Set up the corresponding widget. */
        m_resourceWidget = dynamic_cast<QnMediaResourceWidget *>(m_context->display()->widget(item)); // TODO: check for NULL
        m_resourceWidget->setDisplayFlag(QnResourceWidget::DisplayMotionSensitivity, true);
        m_resourceWidget->setDisplayFlag(QnResourceWidget::DisplayButtons, false);
        m_resourceWidget->setDisplayFlag(QnResourceWidget::DisplayMotion, true);

        /* Find best value for sensitivity. */
        int counts[QnMotionRegion::MAX_SENSITIVITY - QnMotionRegion::MIN_SENSITIVITY + 1];
        memset(counts, 0, sizeof(counts));

        for(int sensitivity = QnMotionRegion::MIN_SENSITIVITY; sensitivity <= QnMotionRegion::MAX_SENSITIVITY; sensitivity++)
            foreach(const QnMotionRegion &motionRegion, m_resourceWidget->motionSensitivity())
                foreach(const QRect &rect, motionRegion.getRectsBySens(sensitivity))
                    counts[sensitivity - QnMotionRegion::MIN_SENSITIVITY] += rect.width() * rect.height();

        int bestCount = std::numeric_limits<int>::min();
        int bestSensitivity = 0;
        for(int sensitivity = QnMotionRegion::MIN_SENSITIVITY; sensitivity <= QnMotionRegion::MAX_SENSITIVITY; sensitivity++) {
            if(counts[sensitivity - QnMotionRegion::MIN_SENSITIVITY] > bestCount) {
                bestCount = counts[sensitivity - QnMotionRegion::MIN_SENSITIVITY];
                bestSensitivity = sensitivity;
            }
        }

        m_motionSensitivity = bestSensitivity;
    }

    /* Consider motion mask list changed. */
    emit motionRegionListChanged();
}

void QnCameraMotionMaskWidget::showTooManyWindowsMessage(const QnMotionRegion &region, const QnMotionRegion::RegionValid kind)
{
    switch(kind){
        case QnMotionRegion::WINDOWS:
            QMessageBox::warning(
                this, 
                tr("Too many motion windows"), 
                tr("Maximum amount of motion windows for current camera is %1, but %2 motion windows are currently selected.")
                    .arg(m_camera->motionWindowCount())
                    .arg(region.getMotionRectCount())
            );
            break;
        case QnMotionRegion::SENS:
            QMessageBox::warning(
                this, 
                tr("Too many motion windows"), 
                tr("Maximum amount of different motion sensitivities for current camera is %1, but %2 motion sensitivities are currently selected.")
                    .arg(m_camera->motionSensWindowCount())
                    .arg(region.getMotionSensCount())
            );
            break;
        case QnMotionRegion::MASKS:
            QMessageBox::warning(
                this, 
                tr("Too many motion windows"), 
                tr("Maximum amount of motion mask windows for current camera is %1, but %2 motion mask windows are currently selected.")
                    .arg(m_camera->motionMaskWindowCount())
                    .arg(region.getMaskRectCount())
            );
            break;
        default:
            break;
    }
}

void QnCameraMotionMaskWidget::setNeedControlMaxRects(bool value)
{
    m_needControlMaxRects = value;
};

int QnCameraMotionMaskWidget::motionSensitivity() const 
{
    return m_motionSensitivity;
}

void QnCameraMotionMaskWidget::setMotionSensitivity(int motionSensitivity) 
{
    m_motionSensitivity = motionSensitivity;
}

void QnCameraMotionMaskWidget::clearMotion() 
{
    at_motionRegionCleared();
}

int QnCameraMotionMaskWidget::gridPosToChannelPos(QPoint &pos) {
    const QnVideoResourceLayout* layout = m_camera->getVideoLayout();
    for (int i = 0; i < layout->numberOfChannels(); ++i) {
        QRect r(layout->h_position(i) * MD_WIDTH, layout->v_position(i) * MD_HEIGHT, MD_WIDTH, MD_HEIGHT);
        if (r.contains(pos)) {
            pos -= r.topLeft();
            return i;
        }
    }
    return 0;
}

bool QnCameraMotionMaskWidget::isValidMotionRegion() {
    if (m_resourceWidget && m_needControlMaxRects) {
        const QnVideoResourceLayout *layout = m_camera->getVideoLayout();
        const QList<QnMotionRegion> &regions = m_resourceWidget->motionSensitivity();
        for (int i = 0; i < qMin(regions.size(), layout->numberOfChannels()); ++i) {
            QnMotionRegion::RegionValid kind = regions[i].isValid(m_camera->motionWindowCount(),
                m_camera->motionMaskWindowCount(), m_camera->motionSensWindowCount());
            if (kind != QnMotionRegion::VALID) {
                showTooManyWindowsMessage(regions[i], kind);
                return false;
            }
        }
    }
    return true;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraMotionMaskWidget::at_viewport_resized() {
    m_context->display()->fitInView(false);
}

void QnCameraMotionMaskWidget::at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *widget, const QRect &gridRect) {
    if (!m_resourceWidget)
        return;

    const QnVideoResourceLayout* layout = m_camera->getVideoLayout();
    bool changed = false;

    int numChannels = layout->numberOfChannels();
    
    // TODO: code that hacks around with channels does not belong here. move to widget.
    for (int i = 0; i < numChannels; ++i) {
        QRect r(0, 0, MD_WIDTH, MD_HEIGHT);
        r.translate(layout->h_position(i)*MD_WIDTH, layout->v_position(i)*MD_HEIGHT);
        r = gridRect.intersected(r);
        r.translate(-layout->h_position(i)*MD_WIDTH, -layout->v_position(i)*MD_HEIGHT);

        if (!r.isEmpty()) {
            QnMotionRegion newRegion = widget->motionSensitivity()[i];
            newRegion.addRect(m_motionSensitivity, r);
            widget->addToMotionSensitivity(i, r, m_motionSensitivity);
            changed = true;
        }
    }
    
    if(changed)
        emit motionRegionListChanged();
}

void QnCameraMotionMaskWidget::at_motionRegionCleared() {
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

void QnCameraMotionMaskWidget::at_itemClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info) {
    if (!m_resourceWidget)
        return;

    QPointF pos = info.scenePos() - item->pos();
    QPoint gridPos = m_resourceWidget->mapToMotionGrid(pos);
    int channel = gridPosToChannelPos(gridPos);
    if (m_resourceWidget->setMotionSensitivityFilled(channel, gridPos, m_motionSensitivity))
        emit motionRegionListChanged();
}


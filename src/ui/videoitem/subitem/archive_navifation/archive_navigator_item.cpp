#include "archive_navigator_item.h"
#include "../../abstract_scene_item.h"
#include "../abstract_image_sub_item.h"

#include "slider_item.h"
#include "base/log.h"
#include "../../video_wnd_archive_item.h"
#include "device_plugins/archive/abstract_archive_stream_reader.h"
#include "ui/graphicsview.h"

int FullScreenHight = 35;

CLArchiveNavigatorItem::CLArchiveNavigatorItem(CLAbstractSubItemContainer* parent, int height):
CLAbstractSubItem(parent, 0.2, 0.8),
mStreamReader(0),
mPlayMode(true),
mSliderIsmoving(false),
mFullScreen(false),
mNonFullScreenHight(height),
mButtonAspectRatio(120.0/70)
{
	m_height = height;
	m_width = parent->boundingRect().width();
	mType = ArchiveNavigatorSubItem;

	/*/
	mPlayItem = new CLImgSubItem(this, ":/skin/try/play2.png", CLAbstractSubItem::Play, 0.7, 1.0, m_height, m_height);
	mPauseItem = new CLImgSubItem(this, ":/skin/try/pause2.png", CLAbstractSubItem::Pause, 0.7, 1.0, m_height, m_height);
	mPauseItem->setVisible(false);

	mSlider_item = new QGraphicsProxyWidget(this);
	mSlider = new CLDirectJumpSlider(Qt::Horizontal);
	mSlider->setRange(0,2000);

	mSlider->setStyleSheet("QSlider { height: 120px}"
	"QSlider::groove:horizontal {"
	"border: 8px solid #0f50af;"
	"background: qlineargradient(x1:0, y1:0, x2:0, y2:4, stop:0 #000030, stop:1 #0000af);"
	"top: 90px;"
	"height: 104px;"
	"margin: 0 0 0 0;}"
	"QSlider::handle:horizontal {"
	"background: qlineargradient(x1:0, y1:0, x2:4, y2:4, stop:0 #0000a0, stop:1 #3358e0);"
	"border: 10px solid #007eff;" // border of handle 
	"width: 120px;"
	"margin: -8px 0 -8px 0;"
	"border-radius: 30px;}");
	/**/

	/**/
    
    int button_width = m_height*mButtonAspectRatio;

	mPlayItem = new CLImgSubItem(this, ":/skin/player/play.png", PlaySubItem, 0.7, 1.0, button_width, m_height);
	mPauseItem = new CLImgSubItem(this, ":/skin/player/pause.png", PauseSubItem, 0.7, 1.0, button_width, m_height);
	mPlayItem->setVisible(false);

	mRewindBackward = new CLImgSubItem(this, ":/skin/player/to_very_beginning.png", RewindBackwardSubItem, 0.5, 1.0, button_width, m_height);
	mRewindForward = new CLImgSubItem(this, ":/skin/player/to_very_end.png", RewindForwardSubItem, 0.5, 1.0, button_width, m_height);

	mStepForward = new CLImgSubItem(this, ":/skin/player/frame_forward.png", StepForwardSubItem, 0.5, 1.0, button_width, m_height);
	mStepForward->setVisible(false);

	mStepBackward = new CLImgSubItem(this, ":/skin/player/frame_backward.png", StepBackwardSubItem, 0.5, 1.0, button_width, m_height);
	mStepBackward->setVisible(false);

	/**/

	mSlider_item = new QGraphicsProxyWidget(this);
	mSlider = new CLDirectJumpSlider(Qt::Horizontal);
	mSlider->setRange(0,2000);
	mSlider->setUpdatesEnabled(false);
	mSlider_item->setWidget(mSlider);
	connect(mSlider, SIGNAL(sliderMoved (int)), this, SLOT(onSliderMoved(int)));
	connect(mSlider, SIGNAL(sliderPressed()), this, SLOT(sliderPressed()));
	connect(mSlider, SIGNAL(sliderReleased()), this, SLOT(sliderReleased()));

	onResize();

	//sliderMoved ( int value )
	//connect(mSlider, SIGNAL(valueChanged(int)), this, SLOT(onSliderMoved(int)));

}

CLArchiveNavigatorItem::~CLArchiveNavigatorItem()
{
	if (mFullScreen)
		goToFullScreenMode(false);
}

void CLArchiveNavigatorItem::goToFullScreenMode(bool fullscreen)
{
	if (mFullScreen == fullscreen) // already in this mode
		return;
	mFullScreen = fullscreen;

	if (mFullScreen)
	{
		QGraphicsView* view = scene()->views().at(0);
		m_width = view->viewport()->width();
		m_height = FullScreenHight;

	}
	else
	{
		m_width = m_parent->boundingRect().width();
		m_height = mNonFullScreenHight;
	}

	QGraphicsView* view = m_parent->scene()->views().at(0);
	GraphicsView* clview = static_cast<GraphicsView*>(view);

	if (mFullScreen)
	{
		setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
		setParentItem(0);

		m_width = view->viewport()->width();
		QPoint pos (0, view->viewport()->height() - m_height);
		setStaticPos(pos);

		clview->addStaticItem(this);

		setPos(view->mapToScene(pos));
		setZValue(256); // on top
	}
	else
	{
		setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
		clview->removeStaticItem(this);
		setParentItem(m_parent);
		setPos((static_cast<CLAbstractSceneItem*>(m_parent))->getBestSubItemPos(mType));
		setZValue(1.0); 
	}

	onResize();
}

// this function uses parent width
void CLArchiveNavigatorItem::onResize()
{
	renewSlider();

	if (mFullScreen)
	{
		QGraphicsView* view = scene()->views().at(0);
		m_width = view->viewport()->width();
		m_height = FullScreenHight;

	}
	else
	{
		m_width = m_parent->boundingRect().width();
		m_height = mNonFullScreenHight;
	}

    int button_width = m_height*mButtonAspectRatio; // this is just very big value; I assume that w>h

	mRewindBackward->setMaxSize(button_width, m_height);
	mPauseItem->setMaxSize(button_width, m_height);
	mPlayItem->setMaxSize(button_width, m_height);
	mRewindForward->setMaxSize(button_width, m_height);

	mStepForward->setMaxSize(button_width, m_height);
	mStepBackward->setMaxSize(button_width, m_height);
	/**/

	const int shift0 = 5;
	const int item_distance = 1;
	const int item_size = item_distance + button_width;

	mRewindBackward->setPos(shift0, 0);
	mPauseItem->setPos(shift0 + item_size,0);
	mPlayItem->setPos(shift0 + item_size,0);
	mRewindForward->setPos(shift0 + 2*item_size, 0);

	mStepForward->setPos(m_width - shift0 - item_size, 0);
	mStepBackward->setPos(m_width - shift0 - 2*item_size, 0);

	int slider_width = m_width - button_width*(3+2+ (m_height==mNonFullScreenHight ? 2.5 : 3));

	mSlider->resize(slider_width, m_height*2/3);
	mSlider_item->setPos(button_width*(3+2), m_height/6);

    //mSlider->resize(slider_width, m_height);
    //mSlider_item->setPos(button_width*(3+2), 0);


	if (m_height==FullScreenHight)
	{

        /*/
        mSlider->setStyleSheet("QSlider::groove:horizontal {"
        "top: 2px;"
        "border: 1px solid #bbb;"
        "background: white;"
        "height: 14px;"
        "border-radius: 4px;}"
        ""
        "QSlider::sub-page:horizontal {"
        "background: qlineargradient(x1: 0, y1: 0,    x2: 0, y2: 1,"
        "stop: 0 #66e, stop: 1 #bbf);"
        "background: qlineargradient(x1: 0, y1: 0.2, x2: 1, y2: 1,"
        "stop: 0 #bbf, stop: 1 #55f);"
        "border: 1px solid #777;"
        "height: 10px;"
        "border-radius: 4px;}"
        ""
        "QSlider::add-page:horizontal {"
        "background: #fff;"
        "border: 1px solid #777;"
        "height: 10px;"
        "border-radius: 4px;}"
        ""
        "QSlider::handle:horizontal {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "stop:0 #eee, stop:1 #ccc);"
        "border: 1px solid #777;"
        "width: 15px;"
        "margin-top: -2px;"
        "margin-bottom: -2px;"
        "border-radius: 4px;}"
        ""
        "QSlider::handle:horizontal:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "stop:0 #fff, stop:1 #ddd);"
        "border: 1px solid #444;"
        "border-radius: 4px;}");


        /**/


        mSlider->resize(slider_width, 28);
        mSlider_item->setPos(button_width*(3+2), 3);


        mSlider->setStyleSheet("QSlider::groove:horizontal {"
                               "top: 0px;"
                               "border: 1px solid #bbb;"
                               "background: white;"
                               "height: 28px;"
                               "border-radius: 4px;}"
                               ""
                               "QSlider::sub-page:horizontal {"
                               "background: qlineargradient(x1: 0, y1: 0,    x2: 0, y2: 1,"
                               "stop: 0 #66e, stop: 1 #99f);"
                               "background: qlineargradient(x1: 0, y1: 0.2, x2: 1, y2: 1,"
                               "stop: 0 #99f, stop: 1 #17f);"
                               "border: 1px solid #777;"
                               "height: 10px;"
                               "border-radius: 4px;}"
                               ""
                               "QSlider::add-page:horizontal {"
                               "background: #fff;"
                               "border: 1px solid #777;"
                               "height: 10px;"
                               "border-radius: 4px;}"
                               ""
                               "QSlider::handle:horizontal {"
                               "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                               "stop:0 #eee, stop:1 #ccc);"
                               "border: 2px solid #777;"
                               "width: 35px;"
                               "margin-top: -2px;"
                               "margin-bottom: -2px;"
                               "border-radius: 4px;}"
                               ""
                               "QSlider::handle:horizontal:hover {"
                               "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                               "stop:0 #fff, stop:1 #ddd);"
                               "border: 2px solid #444;"
                               "border-radius: 4px;}");

	
	}
	else
	{

        mSlider->resize(slider_width, 180);
        mSlider_item->setPos(button_width*(3+2), 10);

        /*/
        mSlider->setStyleSheet("QSlider::groove:horizontal {"
        "border: 4px solid #bbb;"
        "background: white;"
        "height: 90px;"
        "border-radius: 16px;}"
        ""
        "QSlider::sub-page:horizontal {"
        "background: qlineargradient(x1: 0, y1: 0,    x2: 0, y2: 1,"
        "stop: 0 #66e, stop: 1 #bbf);"
        "background: qlineargradient(x1: 0, y1: 0.2, x2: 1, y2: 1,"
        "stop: 0 #bbf, stop: 1 #55f);"
        "border: 4px solid #777;"
        "height: 10px;"
        "border-radius: 14px;}"
        ""
        "QSlider::add-page:horizontal {"
        "background: #fff;"
        "border: 4px solid #777;"
        "height: 10px;"
        "border-radius: 16px;}"
        ""
        "QSlider::handle:horizontal {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "stop:0 #eee, stop:1 #ccc);"
        "border: 4px solid #777;"
        "width: 100px;"
        "margin-top: -8px;"
        "margin-bottom: -8px;"
        "border-radius: 16px;}"
        ""
        "QSlider::handle:horizontal:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "stop:0 #fff, stop:1 #ddd);"
        "border: 4px solid #444;"
        "border-radius: 16px;}");

        /**/

        mSlider->setStyleSheet("QSlider::groove:horizontal {"
                               "border: 4px solid #bbb;"
                               "background: white;"
                               "height: 180px;"
                               "border-radius: 16px;}"
                               ""
                               "QSlider::sub-page:horizontal {"
                               "background: qlineargradient(x1: 0, y1: 0,    x2: 0, y2: 1,"
                               "stop: 0 #66e, stop: 1 #99f);"
                               "background: qlineargradient(x1: 0, y1: 0.2, x2: 1, y2: 1,"
                               "stop: 0 #99f, stop: 1 #17f);"
                               "border: 4px solid #777;"
                               "height: 10px;"
                               "border-radius: 14px;}"
                               ""
                               "QSlider::add-page:horizontal {"
                               "background: #fff;"
                               "border: 4px solid #777;"
                               "height: 10px;"
                               "border-radius: 16px;}"
                               ""
                               "QSlider::handle:horizontal {"
                               "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                               "stop:0 #eee, stop:1 #ccc);"
                               "border: 4px solid #777;"
                               "width: 200px;"
                               "margin-top: -8px;"
                               "margin-bottom: -8px;"
                               "border-radius: 16px;}"
                               ""
                               "QSlider::handle:horizontal:hover {"
                               "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                               "stop:0 #fff, stop:1 #ddd);"
                               "border: 4px solid #444;"
                               "border-radius: 16px;}");


	}

	/**/

}

void CLArchiveNavigatorItem::onSliderMoved(int val)
{
    if (mReader->isSkippingFrames())
        return;

	qreal factor = (qreal)(val) / (mSlider->maximum() - mSlider->minimum());
	quint64 time = mReader->lengthMksec() * factor;

    if (mSliderIsmoving)
        mReader->jumpTo(time, true);
    else
        mReader->jumpToPreviousFrame(time, true);

    m_videoCamera->streamJump();
}

void CLArchiveNavigatorItem::onSubItemPressed(CLAbstractSubItem* subitem)
{
	CLSubItemType type = subitem->getType();
	quint64 curr_time;

	switch(type)
	{
	case PlaySubItem:
		mPlayItem->setVisible(false);
		mPauseItem->setVisible(true);

		mStepBackward->setVisible(false);
		mStepForward->setVisible(false);

		mReader->setSingleShotMode(false);
		mReader->resume();
		mReader->resumeDataProcessors();

		m_videoCamera->getCamCamDisplay()->playAudio(true);

		mPlayMode = true;
		break;

	case PauseSubItem:
		mPlayItem->setVisible(true);
		mPauseItem->setVisible(false);

		mStepBackward->setVisible(true);
		mStepForward->setVisible(true);

		//mReader->pauseDataProcessors();
		mReader->pause();
		m_videoCamera->getCamCamDisplay()->playAudio(false);

		mReader->setSingleShotMode(true);
		mPlayMode = false;
		break;

	case RewindBackwardSubItem:
		mReader->jumpTo(0, true);
        m_videoCamera->streamJump();
        mReader->resumeDataProcessors();
		break;

	case RewindForwardSubItem:
		mReader->jumpTo(mReader->lengthMksec(), true);
        m_videoCamera->streamJump();
        mReader->resumeDataProcessors();
		break;

	case StepForwardSubItem:
        mReader->nextFrame();
		mReader->resume();
		mReader->resumeDataProcessors();
		break;

	case StepBackwardSubItem:
        if (!mReader->isSkippingFrames() && m_videoCamera->currentTime() != 0)
        {
		    curr_time = m_videoCamera->currentTime();
            // cl_log.log("StepBackwardSubItem", int(curr_time/1000), cl_logALWAYS);

            if (mReader->isSingleShotMode())
                m_videoCamera->getCamCamDisplay()->playAudio(false);

            mReader->previousFrame(curr_time);
            m_videoCamera->streamJump();
		    mReader->resumeDataProcessors();
        }

		break;

	default:
		break;
	}

}

void CLArchiveNavigatorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/)
{
	painter->fillRect(boundingRect(),QColor(0, 0, 0));
}

QRectF CLArchiveNavigatorItem::boundingRect() const
{
	return QRectF(0, 0, m_width, m_height);
}

void CLArchiveNavigatorItem::updateSliderPos()
{
	if (mSliderIsmoving)
		return;

    /*
      If reader is skipping frames to time or is already not skipping, but the display
      has not shown any frame - set slider to the position reader is waiting.
      Otherwise use timestamp from display.
     */
	quint64 time;
	if (mReader->isSingleShotMode() || mReader->isSkippingFrames() || m_videoCamera->currentTime() == 0)
		time = mReader->currentTime();
	else
		time = m_videoCamera->currentTime();

	 
	quint64 total = mReader->lengthMksec();

	qreal scale = mSlider->maximum() - mSlider->minimum();

	quint64 pos = (double)time * scale / total;

	mSlider->setValue(pos);
}

void CLArchiveNavigatorItem::sliderPressed()
{
	//cl_log.log("PRESSDED ",  cl_logALWAYS);
	mReader->setSingleShotMode(true);
    m_videoCamera->getCamCamDisplay()->playAudio(false);
	mSliderIsmoving = true;
}

void CLArchiveNavigatorItem::sliderReleased()
{
	//cl_log.log("RELEASED",  cl_logALWAYS);
	mSliderIsmoving = false;

    qreal factor = (qreal)(mSlider->value()) / (mSlider->maximum() - mSlider->minimum());
    quint64 time = mReader->lengthMksec() * factor;

    mReader->previousFrame(time);

	if (mPlayMode)
    {
		mReader->setSingleShotMode(false);
        m_videoCamera->getCamCamDisplay()->playAudio(true);
    }
}

void CLArchiveNavigatorItem::setVideoCamera(CLVideoCamera* camera)
{
	m_videoCamera = camera;
	mReader = static_cast<CLAbstractArchiveReader*>(m_videoCamera->getStreamreader());
}

void CLArchiveNavigatorItem::renewSlider()
{
	if (!m_parent->scene())
		return;

	disconnect(mSlider, SIGNAL(sliderMoved (int)), this, SLOT(onSliderMoved(int)));
	disconnect(mSlider, SIGNAL(sliderPressed()), this, SLOT(sliderPressed()));
	disconnect(mSlider, SIGNAL(sliderReleased()), this, SLOT(sliderReleased()));

	m_parent->scene()->removeItem(mSlider_item);
	delete mSlider_item;
	//delete mSlider;

	mSlider_item = new QGraphicsProxyWidget(this);
	mSlider = new CLDirectJumpSlider(Qt::Horizontal);
	mSlider->setRange(0,2000);
	mSlider->setUpdatesEnabled(false);
	mSlider_item->setWidget(mSlider);

	connect(mSlider, SIGNAL(sliderMoved (int)), this, SLOT(onSliderMoved(int)));
	connect(mSlider, SIGNAL(sliderPressed()), this, SLOT(sliderPressed()));
	connect(mSlider, SIGNAL(sliderReleased()), this, SLOT(sliderReleased()));

}


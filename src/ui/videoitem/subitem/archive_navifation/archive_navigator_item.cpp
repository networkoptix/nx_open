#include "archive_navigator_item.h"
#include "..\..\abstract_scene_item.h"
#include "..\abstract_image_sub_item.h"

#include "slider_item.h"
#include "base\log.h"
#include "..\..\video_wnd_archive_item.h"
#include "device_plugins\archive\abstract_archive_stream_reader.h"
#include "ui\graphicsview.h"

//int NavigatorItemHeight = 200;

CLArchiveNavigatorItem::CLArchiveNavigatorItem(CLAbstractSubItemContainer* parent, int height):
CLAbstractSubItem(parent, 0.2, 0.8),
mStreamReader(0),
mPlayMode(true),
mSliderIsmoving(false),
mFullScreen(false)
{
	m_height = height;
	m_width = parent->boundingRect().width();
	mType = ArchiveNavigatorSubItem;

	/*/
	mPlayItem = new CLImgSubItem(this, "./skin/try/play2.png", CLAbstractSubItem::Play, 0.7, 1.0, m_height, m_height);
	mPauseItem = new CLImgSubItem(this, "./skin/try/pause2.png", CLAbstractSubItem::Pause, 0.7, 1.0, m_height, m_height);
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

	mPlayItem = new CLImgSubItem(this, "./skin/try/play1.png", PlaySubItem, 0.7, 1.0, m_height, m_height);
	mPauseItem = new CLImgSubItem(this, "./skin/try/pause1.png", PauseSubItem, 0.7, 1.0, m_height, m_height);
	mPlayItem->setVisible(false);

	mRewindBackward = new CLImgSubItem(this, "./skin/try/player_rew.png", RewindBackwardSubItem, 0.7, 1.0, m_height, m_height);
	mRewindForward = new CLImgSubItem(this, "./skin/try/player_fwd.png", RewindForwardSubItem, 0.7, 1.0, m_height, m_height);

	mStepForward = new CLImgSubItem(this, "./skin/try/player_end.png", StepForwardSubItem, 0.7, 1.0, m_height, m_height);
	mStepForward->setVisible(false);

	mStepBackward = new CLImgSubItem(this, "./skin/try/player_start.png", StepBackwardSubItem, 0.7, 1.0, m_height, m_height);
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
		m_height = 50;

	}
	else
	{
		m_width = m_parent->boundingRect().width();
		m_height = 400;
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
		m_height = 50;

	}
	else
	{
		m_width = m_parent->boundingRect().width();
		m_height = 400;
	}

	mRewindBackward->setMaxSize(m_height, m_height);
	mPauseItem->setMaxSize(m_height, m_height);
	mPlayItem->setMaxSize(m_height, m_height);
	mRewindForward->setMaxSize(m_height, m_height);

	mStepForward->setMaxSize(m_height, m_height);
	mStepBackward->setMaxSize(m_height, m_height);
	/**/

	const int shift0 = 5;
	const int item_distance = 10;
	const int item_size = item_distance + m_height;

	mRewindBackward->setPos(shift0, 0);
	mPauseItem->setPos(shift0 + item_size,0);
	mPlayItem->setPos(shift0 + item_size,0);
	mRewindForward->setPos(shift0 + 2*item_size, 0);

	mStepForward->setPos(m_width - shift0 - item_size, 0);
	mStepBackward->setPos(m_width - shift0 - 2*item_size, 0);

	int slider_width = m_width - m_height*(3+2+ (m_height==400 ? 2.5 : 3));

	mSlider->resize(slider_width, m_height*2/3);
	mSlider_item->setPos(m_height*(3+2), m_height/6);

	if (m_height==50)
	{

		mSlider->setStyleSheet("QSlider { height: 33px}"
			"QSlider::groove:horizontal {"
			"border: 1px solid #6a6a6a;"
			"background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #6a6a7a, stop:1 #6a6afa);"
			"height: 29px;"
			"margin: 0 0 0 0;}"
			"QSlider::handle:horizontal {"
			"background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #567ecd, stop:1 #567eff);"
			"border: 2px solid #205eff;"
			"width: 33px;"
			"margin: -2px 0 -2px 0;"
			"border-radius: 3px;}");    
	}
	else
	{

		mSlider->setStyleSheet("QSlider { height: 266px}"
			"QSlider::groove:horizontal {"
			"border: 8px solid #6a6a6a;"
			"background: qlineargradient(x1:0, y1:0, x2:0, y2:4, stop:0 #6a6a7a, stop:1 #6a6afa);"
			"height: 260px;"
			"margin: 0 0 0 0;}"
			"QSlider::handle:horizontal {"
			"background: qlineargradient(x1:0, y1:0, x2:4, y2:4, stop:0 #567ecd, stop:1 #567eff);"
			"border: 10px solid #205eff;" // border of handle 
			"width: 266px;"
			"margin: -8px 0 -8px 0;"
			"border-radius: 30px;}");
			/**/

	}

	/**/

}

void CLArchiveNavigatorItem::onSliderMoved(int val)
{
	qreal factor = (qreal)(val) / (mSlider->maximum() - mSlider->minimum());
	quint64 time = mReader->lengthMksec() * factor;

    mReader->jumpTo(time, true);
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

		mReader->resume();
		mReader->resumeDataProcessors();
		break;

	case StepBackwardSubItem:
		curr_time = mReader->currentTime();
		mReader->jumpTo(curr_time - 100 * 1000, true);
		//mReader->setdirection(true);
        m_videoCamera->streamJump();
		mReader->resumeDataProcessors();

		break;

	default:
		break;
	}

}

void CLArchiveNavigatorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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

	quint64 time = mReader->currentTime();
	quint64 total = mReader->lengthMksec();

	qreal scale = mSlider->maximum() - mSlider->minimum();

	quint64 pos = (double)time * scale / total;

	// cl_log.log("SLIDER: ", (int) mReader->currTime(), cl_logALWAYS);

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


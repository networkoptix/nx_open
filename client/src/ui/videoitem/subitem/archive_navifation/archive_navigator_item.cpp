#include "archive_navigator_item.h"
#include "../../abstract_scene_item.h"
#include "../abstract_image_sub_item.h"

#include "slider_item.h"
#include "../../video_wnd_archive_item.h"
#include "ui/graphicsview.h"
#include "ui/skin.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"

int FullScreenHight = 35;

CLArchiveNavigatorItem::CLArchiveNavigatorItem(CLAbstractSubItemContainer* parent, int height) :
    CLAbstractSubItem(parent, 0.2, 0.8),
    m_readerIsSet(false),
    m_nonFullScreenHeight(height),
    m_streamReader(0),
    m_playMode(true),
    m_sliderIsmoving(false),
    m_reader(0),
    m_fullScreen(false),
    m_buttonAspectRatio(120.0/70)
{
	m_height = height;
	m_width = parent->boundingRect().width();
	mType = ArchiveNavigatorSubItem;

	/*/
	mPlayItem = new CLImgSubItem(this, Skin::path(QLatin1String("try/play2.png"), CLAbstractSubItem::Play, 0.7, 1.0, m_height, m_height);
	mPauseItem = new CLImgSubItem(this, Skin::path(QLatin1String("try/pause2.png"), CLAbstractSubItem::Pause, 0.7, 1.0, m_height, m_height);
	mPauseItem->setVisible(false);

	mSlider_item = new QGraphicsProxyWidget(this);
	mSlider = new CLDirectJumpSlider(Qt::Horizontal);
	mSlider->setRange(0,2000);

	mSlider->setStyleSheet(QLatin1String(
        "QSlider { height: 120px}"
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
	    "border-radius: 30px;}"
    ));
	*/

	int button_width = m_height*m_buttonAspectRatio;

	mPlayItem = new CLImgSubItem(this, Skin::path(QLatin1String("player/play.png")), PlaySubItem, 0.7, 1.0, button_width, m_height);
	mPauseItem = new CLImgSubItem(this, Skin::path(QLatin1String("player/pause.png")), PauseSubItem, 0.7, 1.0, button_width, m_height);
	mPlayItem->setVisible(false);

	mRewindBackward = new CLImgSubItem(this, Skin::path(QLatin1String("player/to_very_beginning.png")), RewindBackwardSubItem, 0.5, 1.0, button_width, m_height);
	mRewindForward = new CLImgSubItem(this, Skin::path(QLatin1String("player/to_very_end.png")), RewindForwardSubItem, 0.5, 1.0, button_width, m_height);

	mStepForward = new CLImgSubItem(this, Skin::path(QLatin1String("player/frame_forward.png")), StepForwardSubItem, 0.5, 1.0, button_width, m_height);
	mStepForward->setVisible(false);

	mStepBackward = new CLImgSubItem(this, Skin::path(QLatin1String("player/frame_backward.png")), StepBackwardSubItem, 0.5, 1.0, button_width, m_height);
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
	if (m_fullScreen)
		goToFullScreenMode(false);
}

void CLArchiveNavigatorItem::hideIfNeeded(int duration)
{
    if (!isCursorOnSlider())
    {
        CLAbstractSubItem::hideIfNeeded(duration);
    }
}

bool CLArchiveNavigatorItem::isCursorOnSlider() const
{
    //return isUnderMouse();
    return m_underMouse; //qt bug 18797 When setting the flag ItemIgnoresTransformations for an item, it will receive mouse events as if it was transformed by the view.
}

void CLArchiveNavigatorItem::goToFullScreenMode(bool fullscreen)
{
	if (m_fullScreen == fullscreen) // already in this mode
		return;
	m_fullScreen = fullscreen;

	if (m_fullScreen)
	{
		QGraphicsView* view = scene()->views().at(0);
		m_width = view->viewport()->width();
		m_height = FullScreenHight;

	}
	else
	{
		m_width = m_parent->boundingRect().width();
		m_height = m_nonFullScreenHeight;
	}

	QGraphicsView* view = m_parent->scene()->views().at(0);
	GraphicsView* clview = static_cast<GraphicsView*>(view);

	if (m_fullScreen)
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


void CLArchiveNavigatorItem::resizeSlider()
{
    int sliderHeight = m_height == FullScreenHight ? 28 : 180;

    int buttonWidth = m_height * m_buttonAspectRatio; // this is just very big value; I assume that w>h

    int timeLabelWidth = 0;
    if (m_reader)
    {
        unsigned lengthSecs = m_reader->lengthMksec() / 1000000;

        QFont myFont = mSlider->font();
        myFont.setPixelSize(sliderHeight);
        QFontMetrics fm(myFont);
        timeLabelWidth = fm.width(formatDuration(0, lengthSecs));
    }

    int sliderWidth = m_width - buttonWidth*(3+2+ (m_height==m_nonFullScreenHeight ? 2.5 : 3)) - timeLabelWidth;

    mSlider->resize(sliderWidth, sliderHeight);
}

// this function uses parent width
void CLArchiveNavigatorItem::onResize()
{
	renewSlider();

	if (m_fullScreen)
	{
		QGraphicsView* view = scene()->views().at(0);
		m_width = view->viewport()->width();
		m_height = FullScreenHight;

	}
	else
	{
		m_width = m_parent->boundingRect().width();
		m_height = m_nonFullScreenHeight;
	}

	int button_width = m_height*m_buttonAspectRatio; // this is just very big value; I assume that w>h

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

	resizeSlider();

	mSlider_item->setPos(button_width*(3+2), m_height/6);

    //mSlider->resize(slider_width, m_height);
    //mSlider_item->setPos(button_width*(3+2), 0);


	if (m_height==FullScreenHight)
	{

        /*/
        mSlider->setStyleSheet(QLatin1String(
            "QSlider::groove:horizontal {"
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
            "border-radius: 4px;}"
        ));
       */

        mSlider_item->setPos(button_width*(3+2), 3);

        mSlider->setStyleSheet(QLatin1String(
            "QSlider::groove:horizontal {"
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
            "border-radius: 4px;}"
        ));
	}
	else
	{
		mSlider_item->setPos(button_width*(3+2), 10);

        /*
        mSlider->setStyleSheet(QLatin1String(
            "QSlider::groove:horizontal {"
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
            "border-radius: 16px;}"
        ));
        */

        mSlider->setStyleSheet(QLatin1String(
            "QSlider::groove:horizontal {"
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
            "border-radius: 16px;}"
        ));
	}
}

void CLArchiveNavigatorItem::onSliderMoved(int val)
{
    //if (m_reader->isSkippingFrames())
    //    return;

    qreal factor = (qreal)(val) / (mSlider->maximum() - mSlider->minimum());
    quint64 time = m_reader->lengthMksec() * factor;

    if (m_sliderIsmoving)
        m_reader->jumpTo(time, true);
    else
        m_reader->jumpToPreviousFrame(time);

    //m_videoCamera->streamJump(time);
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

        m_reader->resumeMedia();

        m_videoCamera->getCamCamDisplay()->playAudio(true);

		m_playMode = true;
		break;

	case PauseSubItem:
		mPlayItem->setVisible(true);
		mPauseItem->setVisible(false);

		mStepBackward->setVisible(true);
		mStepForward->setVisible(true);

        //m_reader->pauseDataProcessors();
        m_reader->pause();
        m_videoCamera->getCamCamDisplay()->playAudio(false);

        m_reader->setSingleShotMode(true);
        m_videoCamera->getCamCamDisplay()->setSingleShotMode(true);
        m_playMode = false;
        break;

    case RewindBackwardSubItem:
        m_reader->jumpTo(0, true);
        //m_videoCamera->streamJump(0);
        m_reader->resumeDataProcessors();
        break;

    case RewindForwardSubItem:
        curr_time = m_videoCamera->getCurrentTime();
        m_reader->jumpTo(m_reader->lengthMksec(), true);
        //m_videoCamera->streamJump(m_reader->lengthMksec());
        m_reader->resumeDataProcessors();
        break;

    case StepForwardSubItem:
        m_reader->nextFrame();
        m_videoCamera->getCamCamDisplay()->setSingleShotMode(true);
        //m_reader->resumeDataProcessors();
        break;

    case StepBackwardSubItem:
        if (!m_reader->isSkippingFrames() && m_videoCamera->getCurrentTime() != 0)
        {
            curr_time = m_videoCamera->getCurrentTime();
            // cl_log.log("StepBackwardSubItem", int(curr_time/1000), cl_logALWAYS);

            if (m_reader->isSingleShotMode())
                m_videoCamera->getCamCamDisplay()->playAudio(false);

            m_reader->previousFrame(curr_time);
            //m_videoCamera->streamJump(curr_time);
            m_videoCamera->getCamCamDisplay()->setSingleShotMode(false);
            //m_reader->resumeDataProcessors();
        }

        break;

	default:
		break;
	}
}

void CLArchiveNavigatorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/)
{
    painter->fillRect(boundingRect(), QColor(0, 0, 0));

    if (m_readerIsSet)
    {
        resizeSlider();
        m_readerIsSet = false;
    }

    QFont font = painter->font();
    font.setPixelSize(mSlider->height());
    painter->setPen(QColor(63, 159, 216));
    painter->setFont(font);

    int total = m_reader->lengthMksec() / 1000000;
    int time = mSlider->value() * total / (mSlider->maximum() - mSlider->minimum());

    painter->drawText(mSlider->pos().rx() + mSlider->width() * 1.01 , mSlider->height(), formatDuration(time, total));
}

QRectF CLArchiveNavigatorItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

void CLArchiveNavigatorItem::updateSliderPos()
{
	if (m_sliderIsmoving)
		return;

    /*
      If reader is skipping frames to time or is already not skipping, but the display
      has not shown any frame - set slider to the position reader is waiting.
      Otherwise use timestamp from display.
     */
    quint64 time;
    if (m_reader->isSingleShotMode() || m_reader->isSkippingFrames() || m_videoCamera->getCurrentTime() == 0)
        time = m_reader->currentTime();
    else
        time = m_videoCamera->getCurrentTime();


	quint64 total = m_reader->lengthMksec();

	qreal scale = mSlider->maximum() - mSlider->minimum();

	quint64 pos = (double)time * scale / total;

	mSlider->setValue(pos);
}

void CLArchiveNavigatorItem::sliderPressed()
{
    //cl_log.log("PRESSDED ",  cl_logALWAYS);
    m_reader->setSingleShotMode(true);
    m_videoCamera->getCamCamDisplay()->playAudio(false);
    m_sliderIsmoving = true;
}

void CLArchiveNavigatorItem::sliderReleased()
{
    //cl_log.log("RELEASED",  cl_logALWAYS);
    m_sliderIsmoving = false;

    qreal factor = (qreal)(mSlider->value()) / (mSlider->maximum() - mSlider->minimum());
    quint64 time = m_reader->lengthMksec() * factor;

    m_reader->previousFrame(time);

    if (m_playMode)
    {
        m_reader->setSingleShotMode(false);
        m_videoCamera->getCamCamDisplay()->playAudio(true);
    }
}

void CLArchiveNavigatorItem::setVideoCamera(CLVideoCamera* camera)
{
    m_videoCamera = camera;
    m_reader = static_cast<QnAbstractArchiveReader*>(m_videoCamera->getStreamreader());

    m_readerIsSet = true;
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


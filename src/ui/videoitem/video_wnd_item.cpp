#include "video_wnd_item.h"
#include "base/log.h"
#include "camera/camera.h"
#include "device/device.h"
#include "ui/graphicsview.h"
#include "settings.h"
#include "device_plugins/archive/avi_files/avi_strem_reader.h"

//(videoCodecCtx && videoCodecCtx->sample_aspect_ratio.num) ? av_q2d(videoCodecCtx->sample_aspect_ratio) : 1.0f;

CLVideoWindowItem::CLVideoWindowItem(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height, QString name)
    : CLImageItem(view, max_width, max_height, name),
      m_videolayout(layout),
      m_first_draw(true),
      m_showfps(false),
      m_FPS_Font("Courier New", 250),
      m_opacity(0.0),
      m_videonum(layout->numberOfChannels())
{

    m_aspectratio = m_aspectratio/layout->width()*layout->height();

	m_FPS_Font.setWeight(QFont::Bold);

	m_zoomOnhover = false;

	m_type = VIDEO;
	m_arranged = false;

	memset(m_stat, 0 , sizeof(m_stat));

	//we never cache the video
	//setCacheMode(DeviceCoordinateCache);
	//setCacheMode(ItemCoordinateCache);
	//setFlag(QGraphicsItem::ItemIsMovable);

	for (unsigned i = 0; i  < m_videonum; ++i)
		m_gldraw[i] = new CLGLRenderer(this);

	connect( this, SIGNAL(onAspectRatioChanged(CLAbstractSceneItem*)), this, SLOT(onResize()));
}

CLVideoWindowItem::~CLVideoWindowItem()
{
	for (unsigned i = 0; i  < m_videonum; ++i)
		delete m_gldraw[i];
}

void CLVideoWindowItem::beforeDestroy()
{
	for (unsigned i = 0; i  < m_videonum; ++i)
		m_gldraw[i]->beforeDestroy();;
}

CLVideoCamera* CLVideoWindowItem::getVideoCam() const
{
	return static_cast<CLVideoCamera*>(m_complicatedItem);
}

QSize CLVideoWindowItem::sizeOnScreen(unsigned int /*channel*/) const
{
	return QSize(onScreenSize().width() / m_videolayout->width(), onScreenSize().height() / m_videolayout->height());
}

bool CLVideoWindowItem::constantDownscaleFactor() const
{
    return (m_view->shouldOptimizeDrawing() && !isFullScreen());
}

bool CLVideoWindowItem::isZoomable() const
{
	return true;
}

void CLVideoWindowItem::goToSteadyMode(bool steady, bool instant)
{
    CLImageItem::goToSteadyMode(steady, instant);
    if (steady)
    {
        scene()->views().at(0)->viewport()->setCursor(Qt::BlankCursor);
    }
    else
    {
        scene()->views().at(0)->viewport()->setCursor(Qt::OpenHandCursor);
    }

}

void CLVideoWindowItem::setItemSelected(bool sel, bool animate, int delay )
{
	CLImageItem::setItemSelected(sel, animate, delay);

	if (m_selected)
	{
        if (dynamic_cast<CLAVIStreamReader*>(getVideoCam()->getStreamreader()))
        {
            getVideoCam()->getCamCamDisplay()->setMTDecoding(true);
        }
        
		getVideoCam()->setQuality(CLStreamreader::CLSHighest, true);
		getVideoCam()->getCamCamDisplay()->playAudio(true);
	}
	else
	{
        getVideoCam()->getCamCamDisplay()->setMTDecoding(false);
		getVideoCam()->setQuality(CLStreamreader::CLSNormal, false);
		getVideoCam()->getCamCamDisplay()->playAudio(false);
	}
}

void CLVideoWindowItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
	CLImageItem::hoverEnterEvent(event);

	CLDevice* dev = getComplicatedItem()->getDevice();
	if (!dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
	{
		setShowImagesize(true);
	}

	//if (!dev->checkDeviceTypeFlag(CLDevice::ARCHIVE) && !dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT) )
	if (!dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT) )
	{
		showFPS(true);
		setShowInfoText(true);
	}
}

void CLVideoWindowItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	CLImageItem::hoverLeaveEvent(event);

	if (m_view->getSelectedItem()!=this)
	{
		setShowImagesize(false);
		showFPS(false);
		setShowInfoText(false);
	}
}

void CLVideoWindowItem::draw(CLVideoDecoderOutput& image, unsigned int channel)
{
	// this is not ui thread
	m_first_draw = false;

	m_gldraw[channel]->draw(image, channel); // this function will wait m_gldraw.paintEvent(0);

	//needUpdate(true);
	QMutexLocker locker(&m_mutex);
	m_imageWidth = image.width;
	m_imageHeight = image.height;

	if (m_imageWidth!=m_imageWidth_old || m_imageHeight!=m_imageHeight_old)
	{
		{
			QMutexLocker  locker(&m_mutex_aspect);
			m_aspectratio = qreal(m_imageWidth)/m_imageHeight;

			m_imageWidth_old = m_imageWidth;
			m_imageHeight_old = m_imageHeight;
		}

		emit onAspectRatioChanged(this);
	}
}

QPointF CLVideoWindowItem::getBestSubItemPos(CLSubItemType type)
{
	switch(type)
	{
	case CloseSubItem:
		return QPointF(width()-270, 20);

	case RecordingSubItem:
		return QPointF(-30, 100);

	default:
		return QPointF(-1001, -1001);
	    break;
	}
}

void CLVideoWindowItem::copyVideoDataBeforePainting(bool copy)
{
	for (unsigned i = 0; i  < m_videonum; ++i)
		m_gldraw[i]->copyVideoDataBeforePainting(copy);
}

QRect CLVideoWindowItem::getSubChannelRect(unsigned int channel) const
{
	if (m_videonum==1) // most case scenario 
		return QRect(0,0,width(),height());

	/*

	*----*----*
	|	 |	  |
	|	 |	  |
	*----*----*
	|	 |	  |
	|	 |	  |
	*----*----*

	/**/

	int cel_x = m_videolayout->h_position(channel);
	int cel_y = m_videolayout->v_position(channel);

	int w = width()/m_videolayout->width();
	int h = height()/m_videolayout->height();

	return QRect(cel_x*w, cel_y*h, w,h);
}

void CLVideoWindowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* /*widget*/)
{
	if (m_first_draw)
	{
		return;
	}

	if (painter->paintEngine()->type()	!= QPaintEngine::OpenGL && painter->paintEngine()->type()!= QPaintEngine::OpenGL2) 
	{
		return;
	}

	//painter->setClipping(true);
	//painter->setClipRect( option->exposedRect );

	// save the GL state set for QPainter
    if (m_steadyMode)
        drawSteadyWall(painter);

	painter->beginNativePainting();
	saveGLState();

	for (unsigned i = 0; i  < m_videonum; ++i)
	{
		if (!m_gldraw[i]->paintEvent(getSubChannelRect(i)))
			drawGLfailaure(painter);
	}

	// restore the GL state that QPainter expects

	restoreGLState();

	painter->endNativePainting();
	drawStuff(painter);

	if (option->state & QStyle::State_Selected)
	{
		painter->fillRect(QRect(0, 0, width(), height()),
			m_can_be_droped ? global_can_be_droped_color :  global_selection_color);
	}
}

bool CLVideoWindowItem::wantText() const
{
	return (CLImageItem::wantText() || m_showfps);
}

void CLVideoWindowItem::drawStuff(QPainter* painter)
{
    if (m_steadyMode)
        return;

	CLImageItem::drawStuff(painter);

	drawShadow(painter);

	//============
	if (m_showing_text && m_showfps && global_show_item_text)
	{
		drawFPS(painter);	// ahtung! drawText takes huge(!) ammount of cpu
	}

	if (m_stat[0] && m_stat[0]->isConnectioLost())
		drawLostConnection(painter);

}
void CLVideoWindowItem::drawFPS(QPainter* painter)
{
	painter->setFont(m_FPS_Font);

	//painter->setPen(QColor(0,255,0,170));
	painter->setPen(QColor(0,240,240,170));

	char fps[100];

	for (unsigned i = 0; i < m_videonum; ++i)
	{
		if (m_stat[i]==0)
			continue;

		QRect rect = getSubChannelRect(i);

#ifdef _WIN32
		sprintf_s(fps, "%6.2ffps %6.2fMbps", m_stat[i]->getFrameRate(), m_stat[i]->getBitrate());
#else
		sprintf(fps, "%6.2ffps %6.2fMbps", m_stat[i]->getFrameRate(), m_stat[i]->getBitrate());
#endif
		QFontMetrics fm(painter->font());
		painter->drawText(rect.left()+150,rect.top()+170 + fm.height()/2, fps);
	}
}

void CLVideoWindowItem::drawGLfailaure(QPainter* painter)
{
	painter->setFont(m_FPS_Font);

	QString text;
    QTextStream(&text) << tr("Image size is bigger than MAXGlTextureSize(") << CLGLRenderer::getMaxTextureSize() << ") on this video hardware. Such images cannot be displayed in this version." ;

	QFontMetrics metrics = QFontMetrics(m_FPS_Font);
	int border = qMax(4, metrics.leading());

	QRect rect(0, 0, width() , height());

	painter->fillRect(QRect(0, 0, width(), height()),
		QColor(255, 0, 0, 85));

	painter->setPen(QColor(255, 0, 0, 110));

	painter->drawText((width() - rect.width())/2, border,
		rect.width(), rect.height(),
		Qt::AlignCenter | Qt::TextWordWrap, text);
}

void CLVideoWindowItem::drawLostConnection(QPainter* painter)
{
	painter->setFont(m_FPS_Font);

	QString text = tr("Connection Lost");

	QFontMetrics metrics = QFontMetrics(m_FPS_Font);
	int border = qMax(4, metrics.leading());

	QRect rect(0, 0, width() , height());

	painter->fillRect(QRect(0, 0, width(), height()),
		QColor(255, 0, 0, 85));

	painter->setPen(QColor(255, 0, 0, 110));

	painter->drawText((width() - rect.width())/2, border,
		rect.width(), rect.height(),
		Qt::AlignCenter | Qt::TextWordWrap, text);
}

void CLVideoWindowItem::saveGLState()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	//glMatrixMode(GL_PROJECTION);
	//glPushMatrix();
	//glMatrixMode(GL_MODELVIEW);
	//glPushMatrix();
}

void CLVideoWindowItem::restoreGLState()
{
	//glMatrixMode(GL_PROJECTION);
	//glPopMatrix();
	//glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();
	glPopAttrib();
}

void CLVideoWindowItem::showFPS(bool show)
{
	m_showfps = show;
}

void CLVideoWindowItem::setStatistics(CLStatistics * st, unsigned int channel)
{
	m_stat[channel] = st;
}

float CLVideoWindowItem::aspectRatio() const
{
	QMutexLocker locker(&m_mutex_aspect);

	if (m_videonum == 1) // most case scenario; for optimization
		return m_aspectratio;

	//=============================
	//m_videonum >1
	// at this point we assume that all channels have the same aspect ratio; 
	return m_aspectratio*m_videolayout->width()/m_videolayout->height();
}


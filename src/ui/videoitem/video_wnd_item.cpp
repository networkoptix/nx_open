#include "video_wnd_item.h"
#include "../base/log.h"
#include <QPainter>
#include <QPaintEngine>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include "camera/camera.h"
#include "device/device.h"
#include "ui/graphicsview.h"
#include "settings.h"


CLVideoWindowItem::CLVideoWindowItem(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
									 QString name, QObject* handler):
CLImageItem(view, max_width, max_height, name, handler),
m_videolayout(layout),
m_first_draw(true),
m_showfps(false),
m_FPS_Font("Courier New", 250),
m_opacity(0.0),
m_videonum(layout->numberOfChannels()),
m_cam(0)
{

	m_zoomOnhover = false;

	m_type = VIDEO;
	m_arranged = false;
	
	memset(m_stat, 0 , sizeof(m_stat));
	//we never cache the video
	//setCacheMode(DeviceCoordinateCache);
	//setCacheMode(ItemCoordinateCache);
	//setFlag(QGraphicsItem::ItemIsMovable);
	

	for (int i = 0; i  < m_videonum; ++i)
		m_gldraw[i] = new CLGLRenderer(this);
	
	
	
}


CLVideoWindowItem::~CLVideoWindowItem()
{
	for (int i = 0; i  < m_videonum; ++i)
		delete m_gldraw[i];
}


void CLVideoWindowItem::setVideoCam(CLVideoCamera* cam)
{
	m_cam = cam;
}

CLVideoCamera* CLVideoWindowItem::getVideoCam() const
{
	return m_cam;
}


void CLVideoWindowItem::before_destroy()
{
	for (int i = 0; i  < m_videonum; ++i)
		m_gldraw[i]->before_destroy();;
}


void CLVideoWindowItem::setSelected(bool sel, bool animate, int delay )
{
	CLImageItem::setSelected(sel, animate, delay);

	if (m_selected)
		getVideoCam()->setQuality(CLStreamreader::CLSHighest, true);
	else
		getVideoCam()->setQuality(CLStreamreader::CLSNormal, false);

}

void CLVideoWindowItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{

	CLImageItem::hoverEnterEvent(event);

	CLDevice* dev = getVideoCam()->getDevice();
	if (global_show_item_text && !dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT))
	{
		setShowImagesize(true);
	}

	if (global_show_item_text && !dev->checkDeviceTypeFlag(CLDevice::ARCHIVE) && !dev->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT) )
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
	
	m_first_draw = false;
	m_gldraw[channel]->draw(image, channel); // this function will wait m_gldraw.paintEvent(0);
	//needUpdate(true);
	QMutexLocker locker(&m_mutex);
	m_imageWidth = image.width;
	m_imageHeight = image.height;

	if (m_imageWidth!=m_imageWidth_old || m_imageHeight!=m_imageHeight_old)
	{
		QMutexLocker  locker(&m_mutex_aspect);
		m_aspectratio = qreal(m_imageWidth)/m_imageHeight;
		
		m_imageWidth_old = m_imageWidth;
		m_imageHeight_old = m_imageHeight;

		emit onAspectRatioChanged(this);
	}

}

void CLVideoWindowItem::copyVideoDataBeforePainting(bool copy)
{
	for (int i = 0; i  < m_videonum; ++i)
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

void CLVideoWindowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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
	painter->beginNativePainting();
	saveGLState();
	for (int i = 0; i  < m_videonum; ++i)	m_gldraw[i]->paintEvent(getSubChannelRect(i));

	// restore the GL state that QPainter expects
	drawStuff(painter);
	restoreGLState();
	
	painter->endNativePainting();
	


	/**/
}


bool CLVideoWindowItem::wantText() const
{
	return (CLImageItem::wantText() || m_showfps);
}

void CLVideoWindowItem::drawStuff(QPainter* painter)
{

	CLImageItem::drawStuff(painter);

	//============
	if (m_showing_text && m_showfps)
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

	for (int i = 0; i < m_videonum; ++i)
	{

		if (m_stat[i]==0)
			continue;

		QRect rect = getSubChannelRect(i);

		sprintf(fps, "%6.2ffps %6.2fMbps", m_stat[i]->getFrameRate(), m_stat[i]->getBitrate());
		QFontMetrics fm(painter->font());
		painter->drawText(rect.left(),rect.top()+170 + fm.height()/2, fps);

	}

}

void CLVideoWindowItem::drawLostConnection(QPainter* painter)
{

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
	QMutexLocker  locker(&m_mutex_aspect);

	if (m_videonum==1) // most case scenario; for optimization
		return m_aspectratio;

	//=============================
	//m_videonum >1
	// at this point we assume that all channels have the same aspect ratio; 
	return m_aspectratio*m_videolayout->width()/m_videolayout->height();
}


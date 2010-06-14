#include "video_window.h"
#include "../base/log.h"
#include <QPainter>
#include <QPaintEngine>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>


CLVideoWindow::CLVideoWindow(const CLDeviceVideoLayout* layout, int max_width, int max_height):
m_videolayout(layout),
m_first_draw(true),
m_showfps(true),
m_showinfotext(true),
m_showimagesize(true),
m_FPS_Font("arial black", 250),
m_Info_Font("times", 110),
m_max_width(max_width),
m_max_heght(max_height),
m_imageWidth(0),
m_imageHeight(0),
m_imageWidth_old(0),
m_imageHeight_old(0),
m_opacity(0),
m_videonum(layout->numberOfChannels()),
m_cam(0)


{
	//we never cache the video
	//setCacheMode(DeviceCoordinateCache);
	//setCacheMode(ItemCoordinateCache);

	//setFlag(QGraphicsItem::ItemIsMovable);

	setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
	m_Info_Font.setWeight(QFont::Bold);
	setAcceptHoverEvents(true);

	setFlag(QGraphicsItem::ItemIsFocusable);


	for (int i = 0; i  < m_videonum; ++i)
		m_gldraw[i] = new CLGLRenderer(this);
	
	
	
}


CLVideoWindow::~CLVideoWindow()
{
	for (int i = 0; i  < m_videonum; ++i)
		delete m_gldraw[i];
}

void CLVideoWindow::setVideoCam(CLVideoCamera* cam)
{
	m_cam = cam;
}

CLVideoCamera* CLVideoWindow::getVideoCam() const
{
	return m_cam;
}


void CLVideoWindow::before_destroy()
{
	for (int i = 0; i  < m_videonum; ++i)
		m_gldraw[i]->before_destroy();;
}

void CLVideoWindow::draw(CLVideoDecoderOutput& image, unsigned int channel)
{
	m_first_draw = false;
	m_gldraw[channel]->draw(image, channel); // this function will wait m_gldraw.paintEvent(0);
	QMutexLocker locker(&m_mutex);
	m_imageWidth = image.width;
	m_imageHeight = image.height;

	if (m_imageWidth!=m_imageWidth_old || m_imageHeight!=m_imageHeight_old)
	{
		QMutexLocker  locker(&m_mutex_aspect);
		m_aspectratio = (static_cast<float>(m_imageWidth))/m_imageHeight;
		
		m_imageWidth_old = m_imageWidth;
		m_imageHeight_old = m_imageHeight;

		emit onAspectRatioChanged(this);
	}

}

void CLVideoWindow::copyVideoDataBeforePainting(bool copy)
{
	for (int i = 0; i  < m_videonum; ++i)
		m_gldraw[i]->copyVideoDataBeforePainting(copy);
}


int CLVideoWindow::imageWidth() const
{
	QMutexLocker locker(&m_mutex);
	return m_imageWidth;

}

int CLVideoWindow::imageHeight() const
{
	QMutexLocker locker(&m_mutex);
	return m_imageHeight;
}


void CLVideoWindow::setShowFps(bool show)
{
	m_showfps = show;
}

void CLVideoWindow::setShowInfoText(bool show)
{
	m_showinfotext = show;
}

void CLVideoWindow::setShowImagesize(bool show)
{
	m_showimagesize = show;
}

void CLVideoWindow::setOpacity(int opacity)
{
	m_opacity = opacity;
}

QRect CLVideoWindow::getSubChannelRect(unsigned int channel) const
{
	if (m_videonum==1) // most case scenario 
		return QRect(0,0,width(),height());
		//return QRect(width()/2,height()/2,width()/2,height()/2);

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

void CLVideoWindow::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	
	if (m_first_draw)
	{
		return;
	}

	if (painter->paintEngine()->type()	!= QPaintEngine::OpenGL) 
	{
		return;
	}

	painter->setClipping(true);
	painter->setClipRect( option->exposedRect );
	
	// save the GL state set for QPainter
	saveGLState();

	for (int i = 0; i  < m_videonum; ++i)
			m_gldraw[i]->paintEvent(getSubChannelRect(i));

	

	// restore the GL state that QPainter expects
	restoreGLState();


	if (m_opacity>0)
	{
		painter->fillRect(QRect(0, 0, width(), height()),
			QColor(0, 0, 50, m_opacity));
	}


	
	drawStuff(painter);

	if (m_stat[0]->isConnectioLost())
		drawLostConnection(painter);

	

	/**/
	
}

void CLVideoWindow::applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
	for (int i = 0; i  < m_videonum; ++i)
		m_gldraw[i]->applyMixerSettings(brightness, contrast, hue, saturation);
}

void CLVideoWindow::drawStuff(QPainter* painter)
{
	if (m_showinfotext || m_showimagesize)
		drawInfoText(painter);
	
	if (m_showfps)
	{
		drawFPS(painter);

	}

	
	
}
void CLVideoWindow::drawFPS(QPainter* painter)
{
	painter->setFont(m_FPS_Font);

	painter->setPen(QColor(0,255,0,170));
	
	char fps[100];

	for (int i = 0; i < m_videonum; ++i)
	{

		QRect rect = getSubChannelRect(i);

		sprintf(fps, "%6.2f fps %6.2f Mbps", m_stat[i]->getFrameRate(), m_stat[i]->getBitrate());
		QFontMetrics fm(painter->font());
		painter->drawText(rect.left(),rect.top()+170 + fm.height()/2, fps);

	}

}

void CLVideoWindow::drawLostConnection(QPainter* painter)
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


void CLVideoWindow::drawInfoText(QPainter* painter)
{
	painter->setFont(m_Info_Font);

	//painter->setPen(QColor(255,0,0,220));
	painter->setPen(QColor(30,55,255));

	

	QFontMetrics fm(painter->font());

	QString text;
	QTextStream str(&text);
	
	if (m_showimagesize)
		str << "(" << imageWidth() << "x" << imageHeight() <<") ";

	if (m_showinfotext)
		str << m_infoText;


	painter->drawText(2,2 + fm.height()/2, text);
	
}



void CLVideoWindow::saveGLState()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	//glMatrixMode(GL_PROJECTION);
	//glPushMatrix();
	//glMatrixMode(GL_MODELVIEW);
	//glPushMatrix();
}

void CLVideoWindow::restoreGLState()
{
	//glMatrixMode(GL_PROJECTION);
	//glPopMatrix();
	//glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();
	glPopAttrib();
}

void CLVideoWindow::showFPS(bool show)
{
	m_showfps = show;
}

void CLVideoWindow::setStatistics(CLStatistics * st, unsigned int channel)
{
	m_stat[channel] = st;
}

void CLVideoWindow::setInfoText(QString text)
{
	m_infoText = text;
}

QRectF CLVideoWindow::boundingRect() const
{
	return QRectF(0,0,width(),height());
}

int CLVideoWindow::width() const
{
	if (aspectRatio()>1) // width > hight; normal scenario
		return m_max_width;
	else 
		return m_max_heght*aspectRatio();
}


int CLVideoWindow::height() const
{
	return width()/aspectRatio();
}

float CLVideoWindow::aspectRatio() const
{
	QMutexLocker  locker(&m_mutex_aspect);

	if (m_videonum==1) // most case scenario; for optimization
		return m_aspectratio;

	//=============================
	//m_videonum >1
	// at this point we assume that all channels have the same aspect ratio; 
	return m_aspectratio*m_videolayout->width()/m_videolayout->height();
}

void CLVideoWindow::focusInEvent( QFocusEvent * event )
{
	emit onVideoItemSelected(this);
	QGraphicsItem::focusInEvent(event);
	
}


void CLVideoWindow::mousePressEvent ( QGraphicsSceneMouseEvent * event )
{
	if (event->button()==Qt::RightButton)
	{
		emit onVideoItemMouseRightClick(this);
	}
	QGraphicsItem::mousePressEvent(event);
}
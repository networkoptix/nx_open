#include "video_window.h"
#include "../base/log.h"
#include <QPainter>
#include <QPaintEngine>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>


CLVideoWindow::CLVideoWindow(const CLDeviceVideoLayout* layout):
m_videolayout(layout),
m_first_draw(true),
m_showfps(true),
m_showinfotext(true),
m_showimagesize(true),
m_FPS_Font("arial black", 25),
m_Info_Font("times", 11),
m_max_width(650),
m_imageWidth(0),
m_imageHeight(0),
m_opacity(0),
m_videonum(layout->numberOfChannels())

{
	m_max_heght = m_max_width*3/4;
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
	return QRect(0,0,width(),height());
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
	sprintf(fps, "%6.2f fps %6.2f Mbps", m_stat[0]->getFrameRate(), m_stat[0]->getBitrate());
	QFontMetrics fm(painter->font());
	painter->drawText(0,17 + fm.height()/2, fps);
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
	return m_gldraw[0]->aspectRatio();
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
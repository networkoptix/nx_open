#include "unmoved_pixture_button.h"

#define OPACITY_TIME 500

extern QPixmap cached(const QString &img);

CLUnMovedPixture::CLUnMovedPixture(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, QString img, int max_width, int max_height, qreal z):
CLUnMovedInteractiveOpacityItem(name, parent, normal_opacity, active_opacity),
m_CurrentImg(0),
m_CurrentIndex(0),
m_forwardDirection(true)
{

    if (img.contains(".png"))// single image
    {
        QPixmap pimg = cached(img);
        m_Images.push_back(pimg);
    }
    else
    {
        // set of images 
        QDir dir(img);
        if (!dir.exists())
        {
            Q_ASSERT(false);
            return ;
        }

        dir.setSorting(QDir::Name);

        QStringList filter;
        filter << "*.png";
        QStringList list = dir.entryList(filter);

        foreach(QString file, list)
        {
            QPixmap pimg = cached(img + QString("/") +  file);
            m_Images.push_back(pimg);
        }

        if (m_Images.count()==0)
            return;

    }


    m_CurrentImg = &(m_Images.at(0));

    if (m_Images.count()>1)
    {
        m_Timer.setInterval(1000/15); // 15 fps
        connect(&m_Timer, SIGNAL(timeout()), this, SLOT(onTimer()));
        m_Timer.start();
    }



	setMaxSize(max_width, max_height);
	setZValue(z);

}

CLUnMovedPixture::~CLUnMovedPixture()
{
    m_Timer.stop();
}

void CLUnMovedPixture::onTimer()
{
    if (m_forwardDirection)
    {
        if (m_CurrentIndex >= m_Images.count()-1)
            m_forwardDirection = false;
        else
            ++m_CurrentIndex;

    }
    else
    {
        if (m_CurrentIndex == 0)
            m_forwardDirection = true;
        else
            --m_CurrentIndex;
    }

    m_CurrentImg = &(m_Images.at(m_CurrentIndex));
    update();

}



void CLUnMovedPixture::setFps(int fps)
{
    m_Timer.setInterval(1000/fps); 
}

void CLUnMovedPixture::setMaxSize(int max_width, int max_height)
{
	m_width = m_CurrentImg->width();
	m_height = m_CurrentImg->height();

	qreal aspect = qreal(m_width)/m_height;

	qreal ar_f = (qreal)max_width/max_height;

	if (aspect>ar_f) // width > hight; normal scenario; ar = w/h
	{
		m_width = max_width;
		m_height = m_width/aspect;
	}
	else
	{
		m_width = max_height*aspect;
		m_height = max_height;
	}
}

QSize CLUnMovedPixture::getSize() const
{
	return QSize(m_width, m_height);
}

QRectF CLUnMovedPixture::boundingRect() const
{
	return QRectF(0,0,m_width, m_height);
}

void CLUnMovedPixture::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/)
{
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);

	painter->drawPixmap(boundingRect(), *m_CurrentImg, m_CurrentImg->rect());
}

//========================================================================================================
CLUnMovedPixtureButton::CLUnMovedPixtureButton(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, QString img, int max_width, int max_height, qreal z):
CLUnMovedPixture(name, parent, normal_opacity, active_opacity, img, max_width, max_height,  z)
{

}

CLUnMovedPixtureButton::~CLUnMovedPixtureButton()
{

}

void CLUnMovedPixtureButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* /*event*/)
{
	emit onPressed(m_name);
}

void CLUnMovedPixtureButton::mousePressEvent(QGraphicsSceneMouseEvent* /*event*/)
{
}

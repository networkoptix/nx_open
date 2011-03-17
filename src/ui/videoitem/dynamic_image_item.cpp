#include "dynamic_image_item.h"


extern QPixmap cached(const QString &img);

CLDynamicImageItem::CLDynamicImageItem(GraphicsView* view, int max_width, int max_height,
				   QString path, 
				   QString name):
CLImageItem(view, max_width,max_height,name),
m_CurrentImg(0),
m_CurrentIndex(0),
m_forwardDirection(true)
{

    QDir dir(path);
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
        QPixmap img = cached(path + QString("/") +  file);
        m_Images.push_back(img);
    }

    if (m_Images.count()==0)
        return;


    m_CurrentImg = &(m_Images.at(0));

	m_imageWidth_old = m_imageWidth = m_CurrentImg->width();
	m_imageHeight_old = m_imageHeight = m_CurrentImg->height();
	m_aspectratio = qreal(m_imageWidth)/m_imageHeight;


    m_Timer.setInterval(1000/15); // 25 fps
    connect(&m_Timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    m_Timer.start();

}

CLDynamicImageItem::~CLDynamicImageItem()
{
    m_Timer.stop();
}

void CLDynamicImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (!m_CurrentImg)
        return;

	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);

	painter->drawPixmap(boundingRect(), *m_CurrentImg, m_CurrentImg->rect());
	drawStuff(painter);
}

void CLDynamicImageItem::onTimer()
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

//

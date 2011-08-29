#include "picture_image_item.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include "ui/ui_common.h"

CLPictureImageItem::CLPictureImageItem(GraphicsView* view, int max_width, int max_height,
				   QString path,
				   QString name):
CLImageItem(view, max_width,max_height,name),
m_CurrentImg(0),
m_CurrentIndex(0),
m_forwardDirection(true)
{

    if (path.endsWith(QLatin1String(".png")))
    {
        // single image
        m_Images.append(cached(path));
    }
    else
    {
        // set of images
        QDir dir(path);
        const QStringList filter = QStringList() << QLatin1String("*.png");
        foreach (const QFileInfo &fi, dir.entryInfoList(filter, QDir::Files, QDir::Name))
            m_Images.append(cached(fi.absoluteFilePath()));

        if (m_Images.isEmpty())
            m_Images.append(QPixmap());
    }

    m_CurrentImg = &m_Images.at(0);

    m_imageWidth_old = m_imageWidth = m_CurrentImg->width();
    m_imageHeight_old = m_imageHeight = m_CurrentImg->height();
    m_aspectratio = qreal(m_imageWidth)/m_imageHeight;

    if (m_Images.count()>1)
    {
        m_Timer.setInterval(1000/200); // 25 fps
        connect(&m_Timer, SIGNAL(timeout()), this, SLOT(onTimer()));
        m_Timer.start();
    }


}

CLPictureImageItem::~CLPictureImageItem()
{
    m_Timer.stop();
}

void CLPictureImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem */*option*/, QWidget */*widget*/)
{
    if (!m_CurrentImg)
        return;

	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->setRenderHint(QPainter::Antialiasing);

	painter->drawPixmap(boundingRect(), *m_CurrentImg, m_CurrentImg->rect());
	drawStuff(painter);
}

void CLPictureImageItem::onTimer()
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

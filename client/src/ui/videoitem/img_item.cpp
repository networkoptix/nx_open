#include "img_item.h"

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include "ui/style/globals.h"

CLImageItem::CLImageItem(GraphicsView* view, int max_width, int max_height, QString name)
    : CLAbstractSceneItem(view, max_width, max_height,name),
    m_imageWidth(m_max_width),
    m_imageHeight(m_max_height),
    m_imageWidth_old(m_max_width),
    m_imageHeight_old(m_max_height),
    m_aspectratio((qreal)m_imageWidth/m_imageHeight),
    m_showimagesize(false),
    m_showinfotext(false),
    m_showing_text(false),
    m_timeStarted(false),
    m_Info_Font(QLatin1String("times"), 110)
{
    m_type = IMAGE;
    m_Info_Font.setWeight(QFont::Bold);
}

int CLImageItem::height() const
{
    return width()/aspectRatio();
}

int CLImageItem::width() const
{
    float ar = aspectRatio();
    float ar_f = (float)m_max_width / m_max_height;

    // width > hight; normal scenario; ar = w/h
    return ar > ar_f ? m_max_width : m_max_height * ar;
}

float CLImageItem::aspectRatio() const
{
    QMutexLocker locker(&m_mutex_aspect);
    return m_aspectratio;
}

QSize CLImageItem::imageSize() const
{
    QMutexLocker locker(&m_mutex);
    return QSize(m_imageWidth, m_imageHeight);
}

bool CLImageItem::showImageSize() const
{
    return m_showimagesize;
}

void CLImageItem::setShowImageSize(bool show)
{
    m_showimagesize = show;
}

QString CLImageItem::infoText() const
{
    return m_infoText;
}

void CLImageItem::setInfoText(const QString &text)
{
    m_infoText = text;
}

bool CLImageItem::showInfoText() const
{
    return m_showinfotext;
}

void CLImageItem::setShowInfoText(bool show)
{
    m_showinfotext = show;
}

QString CLImageItem::extraInfoText() const
{
    return m_extraInfoText;
}

void CLImageItem::setExtraInfoText(const QString &text)
{
    m_extraInfoText = text;
}

bool CLImageItem::wantText() const
{
    return m_showimagesize || m_showinfotext || !m_extraInfoText.isEmpty();
}

void CLImageItem::drawStuff(QPainter* painter)
{
    if (!wantText())
    {
        m_showing_text = false;
        m_timeStarted = false;
    }
    else if (!m_showing_text)
    {
        if (m_timeStarted)
        {
            int ms = m_textTime.msecsTo(QTime::currentTime());
            if (ms>350)
                m_showing_text = true;

        }
        else
        {
            m_textTime.restart();
            m_timeStarted = true;
        }
    }
    //===================================

    if (m_showing_text)
        drawInfoText(painter); // ahtung! drawText takes huge(!) ammount of cpu

    if (m_draw_rotation_helper)
        drawRotationHelper(painter);
}

void CLImageItem::drawInfoText(QPainter* painter)
{
    // ignore Globals::showItemText()
    if (!m_extraInfoText.isEmpty())
    {
        painter->setFont(QFont(QLatin1String("Times"), 150));
        painter->setPen(QColor(218, 180, 130));

        QRectF r = painter->boundingRect(boundingRect(), Qt::AlignLeft | Qt::AlignTop, m_extraInfoText);
        r.moveTopLeft(QPointF(5, 5));
        if (Globals::showItemText() && (m_showimagesize || m_showinfotext))
            r.moveTop(20 + QFontMetrics(m_Info_Font).height());

        painter->drawText(r, m_extraInfoText);
    }

    if (!Globals::showItemText())
        return;

    painter->setFont(m_Info_Font);
    //painter->setPen(QColor(255,0,0,220));
    painter->setPen(QColor(30,55,255));

    QString text;
    if (m_showimagesize)
    {
        const QSize sz = imageSize();
        text += QLatin1Char('(') + QString::number(sz.width()) + QLatin1Char('x') + QString::number(sz.height()) + QLatin1String(") ");
    }
    if (m_showinfotext)
        text += m_infoText;

    QFontMetrics fm(painter->font());
    painter->drawText(2, 20 + fm.height() / 2, text);
}

#include "page_subitem.h"
#include "settings.h"
#include "ui/animation/property_animation.h"
#include "base/log.h"

//static QFont  page_font("Courier New", 20);
static QFont page_font("Times", 20, QFont::Bold);
static const int adjustment = 3;

static const QColor normalColor(0,0,255);
static const QColor activeColor(0,240,240,170);
 
static const int animationPeriod = 350;


QnPageSubItem::QnPageSubItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, int number, bool current):
CLUnMovedInteractiveOpacityItem(name, parent, normal_opacity, active_opacity),
m_number(number),
m_scaleAnimation(0),
m_colorAnimation(0),
m_current(current),
m_firstPaint(true)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations, false);

    QFontMetrics metrics = QFontMetrics(page_font);

    m_boundingRect = QRectF(0, 0, metrics.width(name), metrics.height());
    m_boundingRect.adjust( -adjustment, -adjustment, adjustment, adjustment);

    setTransformOriginPoint(boundingRect().center() + QPointF(0,boundingRect().height()/2));

    /*
    if (m_current)
    {
        setScale(1.3);
        m_color = activeColor;
    }
    else
    {
        m_color = normalColor;
    }
    /**/

    m_color = normalColor;

    
}

QnPageSubItem::~QnPageSubItem()
{
    stopAnimation();
    stopScaleAnimation();
    stopColorAnimation();
}

QRectF QnPageSubItem::boundingRect () const
{
    return m_boundingRect;
}

//==========================================================================

void QnPageSubItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setFont(page_font);
    painter->setPen(m_color);
    painter->drawText(0, m_boundingRect.height() - adjustment, getName());

    //painter->fillRect(boundingRect(), QColor(0, 255,0, 128));

    if (m_firstPaint && m_current)
    {
        animateScale(1.3, animationPeriod);
        animateColor(activeColor, animationPeriod);
    }

    m_firstPaint = false;
}


void QnPageSubItem::stopScaleAnimation()
{
    if (m_scaleAnimation)
    {
        m_scaleAnimation->stop();
        delete m_scaleAnimation;
        m_scaleAnimation = 0;
    }

}

void QnPageSubItem::stopColorAnimation()
{
    if (m_colorAnimation)
    {
        m_colorAnimation->stop();
        delete m_colorAnimation;
        m_colorAnimation = 0;
    }
}

QColor QnPageSubItem::color() const
{
    return m_color;
}

void QnPageSubItem::setColor(QColor c)
{
    m_color = c;
}


void QnPageSubItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_current)
        return; // no animation needed

    CLUnMovedInteractiveOpacityItem::hoverEnterEvent(event);
    animateScale(1.6, animationPeriod);
    animateColor(activeColor, animationPeriod);

}

void QnPageSubItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_current)
        return; // no animation needed

    CLUnMovedInteractiveOpacityItem::hoverLeaveEvent(event);
    animateScale(1.0, animationPeriod);
    animateColor(normalColor, animationPeriod);
}

void QnPageSubItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent *e)
{
    CLUnMovedInteractiveOpacityItem::mouseReleaseEvent(e);
    //cl_log.log("mouseReleaseEvent", cl_logALWAYS);
    emit onPageSelected(m_number);
}

void QnPageSubItem::mousePressEvent ( QGraphicsSceneMouseEvent *e)
{
    CLUnMovedInteractiveOpacityItem::mousePressEvent(e);
    e->accept();
    //cl_log.log("mousePressEvent", cl_logALWAYS);
}


void QnPageSubItem::animateScale(qreal sc, unsigned int duration)
{
    stopScaleAnimation();


    if (duration==0)
    {
        setScale(sc);
        return;
    }

    m_scaleAnimation = AnimationManager::instance().addAnimation(this, "scale");
    m_scaleAnimation->setEasingCurve(QEasingCurve::OutBack);
    m_scaleAnimation->setDuration(duration);
    m_scaleAnimation->setStartValue(scale());
    m_scaleAnimation->setEndValue(sc);
    m_scaleAnimation->start();	
    connect(m_scaleAnimation, SIGNAL(finished ()), this, SLOT(stopScaleAnimation()));

}

void QnPageSubItem::animateColor(QColor c, unsigned int duration)
{
    stopColorAnimation();


    if (duration==0)
    {
        setColor(c);
        return;
    }

    m_colorAnimation = AnimationManager::instance().addAnimation(this, "color");
    m_colorAnimation->setEasingCurve(QEasingCurve::OutBack);
    m_colorAnimation->setDuration(duration);
    m_colorAnimation->setStartValue(color());
    m_colorAnimation->setEndValue(c);
    m_colorAnimation->start();	
    connect(m_colorAnimation, SIGNAL(finished ()), this, SLOT(stopColorAnimation()));

}
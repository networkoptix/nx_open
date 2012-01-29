#include <math.h>

#include "scene_zoom.h"
#include "ui/widgets/graphicsview.h"
#include "settings.h"
#include "ui/videoitem/abstract_scene_item.h"
#include "ui/animation/property_animation.h"
#include "ui/style/globals.h"


static const qreal max_zoom = 4.0;
static const qreal def_zoom = 0.22;

static const qreal low_normal_quality_zoom = 0.22;
static const qreal normal_hight_quality_zoom = 0.30;
static const qreal hight_highest_quality_zoom = 0.32;

CLSceneZoom::CLSceneZoom(GraphicsView* gview):
    CLAnimation(gview),
    m_zoom(def_zoom+0.1),
    m_targetzoom(0),
    m_quality(QnQualityNormal)
{
}

CLSceneZoom::~CLSceneZoom()
{
    stopAnimation();
}

qreal CLSceneZoom::zoomToscale(qreal zoom) const
{
    return zoom*zoom;
}

qreal CLSceneZoom::scaleTozoom(qreal scale) const
{
    return sqrt(scale);
}

qreal CLSceneZoom::getZoom() const
{
    return m_zoom;
}

void CLSceneZoom::setZoom(qreal z)
{
    m_zoom = z;

    qreal scl = zoomToscale(m_zoom);

    QPoint unmoved_viewpos;
    if (m_unmoved_point!=QPoint(0,0))
    {
        // first of all lets get m_unmoved_point view pos
        unmoved_viewpos = m_view->mapFromScene(m_unmoved_point);
    }

    QTransform tr;
    tr.scale(scl, scl);
    tr.rotate(Globals::rotationAngle(), Qt::YAxis);

    m_skipViewUpdate = m_view->transform() != tr;
    m_view->setTransform(tr);

    if (m_unmoved_point!=QPoint(0,0))
    {
        // first of all lets get m_unmoved_point view pos
        QPoint delta = unmoved_viewpos - m_view->mapFromScene(m_unmoved_point);
        m_view->viewMove(delta.x(), delta.y());
    }
    else // viewMove already called addjustAllStaticItems; do not need to do it second time
    {
        m_view->adjustAllStaticItems();
    }

    //=======================================

    bool zooming_out = (m_targetzoom < getZoom()) && qAbs(m_targetzoom - getZoom())/m_targetzoom > 0.03;

    //if (m_view->getSelectedItem() &&  zooming_out  && getZoom()<0.260) // if zooming out only
    if (m_view->getSelectedItem() &&  zooming_out) // if zooming out only
    {
        QRectF item_rect = m_view->getSelectedItem()->sceneBoundingRect();
        QRectF view_rect = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();

        if ( item_rect.width() < view_rect.width()*0.95 &&
            item_rect.height() < view_rect.height()*0.95)
            m_view->getSelectedItem()->setFullScreen(false);

        if ( view_rect.width()/item_rect.width() > 3.2  &&
            view_rect.height()/item_rect.height() > 3.2)
            m_view->setZeroSelection();

    }

    if (zooming_out)
    {
        if (m_quality==QnQualityHighest && getZoom() <= hight_highest_quality_zoom)
        {
            m_view->setAllItemsQuality(QnQualityHigh, false);
            m_quality = QnQualityHigh;
        }

        if (m_quality==QnQualityHigh && getZoom() <= normal_hight_quality_zoom)
        {
            m_view->setAllItemsQuality(QnQualityNormal, false);
            m_quality = QnQualityNormal;
        }

        else if (m_quality==QnQualityNormal&& getZoom() <= low_normal_quality_zoom)
        {
            m_view->setAllItemsQuality(QnQualityLow, false);
            m_quality = QnQualityLow;
        }
    }
    else// zoomin in
    {
        if (m_quality==QnQualityLow && getZoom() > low_normal_quality_zoom)
        {
            m_view->setAllItemsQuality(QnQualityNormal, true);
            m_quality = QnQualityNormal;
        }

        else if (m_quality==QnQualityNormal && getZoom() > normal_hight_quality_zoom)
        {
            m_view->setAllItemsQuality(QnQualityHigh, true);
            m_quality = QnQualityHigh;
        }

        else if (m_quality==QnQualityHigh && getZoom() > hight_highest_quality_zoom)
        {
            m_view->setAllItemsQuality(QnQualityHighest, true);
            m_quality = QnQualityHighest;
        }

    }

    //#include "camera\camera.h"

}


void CLSceneZoom::zoom_abs(qreal z, int duration, int delay, QPoint unmoved_point, CLAnimationCurve curve)
{
    m_skipViewUpdate = false;

    m_unmoved_point = unmoved_point;
    m_targetzoom = z;
    zoom_helper(duration, delay, curve);
}

void CLSceneZoom::zoom_minimum(int duration, int delay, QPoint unmoved_point, CLAnimationCurve curve)
{
    m_skipViewUpdate = false;

    m_unmoved_point = unmoved_point;
    zoom_abs(m_view->getMinSceneZoom() , duration, delay, QPoint(0,0), curve);
}

void CLSceneZoom::zoom_delta(qreal delta, int duration, int delay, QPoint unmoved_point, CLAnimationCurve curve)
{
    m_skipViewUpdate = false;

    m_unmoved_point = unmoved_point;
    m_targetzoom += delta;
    zoom_helper(duration, delay, curve);
}

//============================================================

void CLSceneZoom::set_quality_helper()
{

}

void CLSceneZoom::zoom_helper(int duration, int delay, CLAnimationCurve curve)
{
    QMutexLocker _locker(&m_animationMutex);

    stopAnimation();

    if (m_targetzoom>max_zoom)
        m_targetzoom = max_zoom;

    if (m_targetzoom < m_view->getMinSceneZoom())
        m_targetzoom = m_view->getMinSceneZoom();

    if (duration==0)
    {
        m_zoom = m_targetzoom;
        setZoom(m_zoom);
    }
    else
    {

        m_animation = AnimationManager::addAnimation(this, "zoom");

        QPropertyAnimation* panimation = static_cast<QPropertyAnimation*>(m_animation);
        panimation->setStartValue(this->getZoom());
        panimation->setEndValue(m_targetzoom);
        panimation->setDuration(duration);

        panimation->setEasingCurve(easingCurve(curve));

        start_helper(duration, delay);
    }

}

#include "scene_movement.h"

#include "ui/widgets/graphicsview.h"
#include "ui/videoitem/video_wnd_item.h"
#include "ui/animation/property_animation.h"

int limit_val(int val, int min_val, int max_val, bool mirror)
{

    if (!mirror)
    {
        if (val > max_val)
            return max_val;

        if (val < min_val)
            return min_val;
    }

    int width = max_val - min_val;

    if (val > max_val)
    {
        int x = val - min_val;

        x = (x%(2*width));// now x cannot be more than 2*width
        x+=min_val;

        if (x > max_val)
        {
            int diff = x - max_val; // so many bigger
            x = max_val - diff; // now x cannot be more than 2*width
        }

        return x;
    }

    if (val < min_val)
    {

        int x = max_val - val;
        x = x%(2*width);
        x = max_val - x;

        if (x < min_val)
        {
            int diff = min_val - x; // so many smaller
            x = min_val + diff;
        }

        return x;

    }

    return val;
}

//========================================================================================

CLSceneMovement::CLSceneMovement(GraphicsView* gview):
    CLAnimation(gview),
    m_view(gview),
    m_limited(false)
{

}

CLSceneMovement::~CLSceneMovement()
{
    stopAnimation();
}

void CLSceneMovement::move(int dx, int dy, int duration, bool limited, int delay, CLAnimationCurve curve  )
{

    QPoint curr = m_view->viewport()->rect().center();
    curr.rx()+=dx;
    curr.ry()+=dy;

    move(m_view->mapToScene(curr), duration, delay, curve);

    m_limited = limited; // this will overwrite false value set inside move_abs function above
}

void CLSceneMovement::move (QPointF dest, int duration, int delay, CLAnimationCurve curve )
{
    QMutexLocker _locker(&m_animationMutex);

    //cl_log.log("CLSceneMovement::move() ", cl_logDEBUG1);
    stopAnimation();

    m_limited = false;

    if (duration==0)
    {
        setPosition(dest);
    }
    else
    {
        m_startpoint = getPosition();
        m_delta = dest - m_startpoint;

        m_animation = AnimationManager::addAnimation(this, "position");
        QPropertyAnimation* panimation = static_cast<QPropertyAnimation*>(m_animation);
        panimation->setStartValue(getPosition());
        panimation->setEndValue(dest);
        panimation->setDuration(duration);

        panimation->setEasingCurve(easingCurve(curve));

        start_helper(duration, delay);
    }
}

QPointF CLSceneMovement::getPosition() const
{
    return m_view->mapToScene(m_view->viewport()->rect().center());
}

void CLSceneMovement::setPosition(QPointF p)
{
    QRect rsr = m_view->getRealSceneRect();

    p.rx() = limit_val(p.x(), rsr.left(), rsr.right(), false);
    p.ry() = limit_val(p.y(), rsr.top(), rsr.bottom(), false);

    m_view->centerOn(p);

    //=======================================
    if (m_limited && m_view->getSelectedItem())
    {
        CLAbstractSceneItem* item = m_view->getSelectedItem();

        QPointF wnd_center = item->mapToScene(item->boundingRect().center());
        QPointF final_dest = m_startpoint + m_delta;
        QPointF curr = m_view->mapToScene(m_view->viewport()->rect().center());

        if (QLineF(final_dest, wnd_center).length() > QLineF(curr, wnd_center).length()   )
        {
            // if difference between final point and item center is more than curr and item center => moving out of item center
//            int dx = curr.x() - wnd_center.x();
//            int dy = curr.y() - wnd_center.y();

//            qreal percent = 0.05;

//            QRectF wnd_rect =  item->sceneBoundingRect();
//            QRectF viewport_rec = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();

            // animation cannot be stoped inside animation
            /*/
            if (dx<0)
            {
                if ( (wnd_rect.topLeft().x() -  viewport_rec.topLeft().x()) >  percent*wnd_rect.width())
                    stopAnimation();
            }

            if (dx>0)
            {
                if ( (viewport_rec.topRight().x() - wnd_rect.topRight().x()) >  percent*wnd_rect.width())
                    stopAnimation();
            }

            if (dy<0)
            {
                if ( (wnd_rect.topLeft().y() - viewport_rec.topLeft().y()) >  percent*wnd_rect.height())
                    stopAnimation();
            }

            if (dy>0)
            {
                if ( (viewport_rec.bottomLeft().y()- wnd_rect.bottomRight().y()) >  percent*wnd_rect.height())
                    stopAnimation();
            }
            */

        }

    }
    else if (m_view->getSelectedItem())
    {
        CLAbstractSceneItem* item = m_view->getSelectedItem();

        QPointF selected_center = item->mapToScene(item->boundingRect().center());
        QPointF dest = m_startpoint + m_delta;

        QPointF diff = selected_center - dest;

        if ( abs(diff.x())< 1.0 &&  abs(diff.y())< 1.0 )
            return; // moving to the selection => do not need to unselect

        // else
        diff = selected_center - p;

        if ( abs(diff.x()) > item->boundingRect().width()*1.0 || abs(diff.y()) > item->boundingRect().height()*1.0)
            m_view->setZeroSelection();
    }

}



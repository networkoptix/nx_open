#include "resource_server_widget.h"

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include "core/resource/video_server.h"

#define LIMIT 60
#define REQUEST_TIME 2000

QnResourceServerWidget::QnResourceServerWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /* = NULL */):
    QnResourceWidget(context, item, parent),
    m_api_connection(0),
    m_alreadyUpdating(false),
    m_redrawStatus(Qn::LoadingFrame){
        for (int i = 0; i < LIMIT; i++)
            m_history.append(-1);
        m_steps.append(1);
        m_steps.append(2);
        m_steps.append(3);
        m_steps.append(4);
        m_steps.append(5);
        m_steps.append(10);
        m_steps.append(15);
        m_steps.append(20);
        m_steps.append(25);
        m_steps.append(30);
        m_steps.append(40);
        m_steps.append(50);
        m_steps.append(60);
        m_steps.append(70);
        m_steps.append(80);
        m_steps.append(90);
        m_steps.append(100);

        m_timer = new QTimer(this);
        connect(m_timer, SIGNAL(timeout()), this, SLOT(at_timer_update()));
        m_timer->start(REQUEST_TIME);

        m_background_gradient.setColorAt(0.0, QColor(0, 0, 195, 100));
        m_background_gradient.setColorAt(1, QColor(0, 0, 0, 0));

}

void QnResourceServerWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
    if (!m_api_connection){
        QnVideoServerResourcePtr server = resource().dynamicCast<QnVideoServerResource>();
        Q_ASSERT(server);
        m_api_connection = server->apiConnection();
    }
    assertPainters();

    switch (m_redrawStatus)
    {
    case Qn::NewFrame:
        setOverlayIcon(0, NO_ICON);
        // update cache here
    	break;
    case Qn::OldFrame:
        break;
    case Qn::LoadingFrame:
        setOverlayIcon(0, LOADING);
        break;
    case Qn::CannotLoad:
        setOverlayIcon(0, NO_DATA);
        break;
    }
    drawStatistics(rect().width(), rect().height(), painter);

    painter->beginNativePainting();
    //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawOverlayIcon(0, rect());
    glDisable(GL_BLEND);
    //glPopAttrib();
    painter->endNativePainting();
}

void QnResourceServerWidget::at_timer_update(){
    if (m_alreadyUpdating){
        if (m_redrawStatus != Qn::CannotLoad)
            m_redrawStatus = Qn::LoadingFrame;
    }
    m_alreadyUpdating = true;
    QnVideoServerResourcePtr server = resource().dynamicCast<QnVideoServerResource>();
    Q_ASSERT(server);
    int handle = server->apiConnection()->asyncGetStatistics(this, SLOT(at_statistics_received(int)));
}

void QnResourceServerWidget::at_statistics_received(int usage){
    m_alreadyUpdating = false;
    m_redrawStatus = Qn::NewFrame;
    m_history.push_back(usage);
    m_elapsed_timer.restart();
    if (m_history.count() > LIMIT)
        m_history.pop_front();
}

QPainterPath scalePath(const QPainterPath &source, qreal scaleFactor, qreal offset, qreal height){
    QPainterPath path;
    path.addPath(source);
    //   qreal flip = m_intensity / qreal(100);
    for (int i=0; i<path.elementCount(); ++i)  {
        const QPainterPath::Element &e = path.elementAt(i);
        qreal x = e.x;
        qreal y = offset + height * (1.0 - e.y * scaleFactor);
        path.setElementPositionAt(i, x, y);
    }
    return path;
}

void QnResourceServerWidget::drawStatistics(int width, int height, QPainter *painter){
    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

   //painter->setOpacity(.5);
    qreal min = qMin(width, height);

    bool stretch_y = false;
    bool grid_enabled = true;
    bool background_enabled = true;
    qreal offset = min / 20.0;
    qreal pen_width = width / 500.0;

    qreal scale_factor = 1.0;

    QRectF rect(0, 0, width, height);
    qreal oh = height - offset*2;
    qreal ow = width - offset*2;

    QRectF inner(offset, offset, ow, oh);

    painter->fillRect(rect, Qt::black);
    painter->setRenderHint(QPainter::Antialiasing);

    qreal radius = min * 0.5;
    QPointF center(width * 0.5, height * 0.5);
    m_background_gradient.setCenter(center);
    m_background_gradient.setFocalPoint(center);
    m_background_gradient.setRadius(radius);
    painter->setBrush(m_background_gradient);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(width/2, height/2), radius, radius);

    painter->setClipRect(inner);    

    if (grid_enabled){
        QPen grid;
        grid.setColor(QColor(0, 75, 190, 100));
        grid.setWidthF(pen_width);


        const qreal step = ow / 20.0;
        QPainterPath grid_path;
        for (qreal i = step/2; i < ow; i += step){
            grid_path.moveTo(i, offset);
            grid_path.lineTo(i, oh + offset);
        }
        for (qreal i = step/2; i < oh; i += step){
            grid_path.moveTo(offset, i);
            grid_path.lineTo(ow + offset, i);
        }

        painter->strokePath(grid_path, grid);
    }


    QPen usage;{
        /*QLinearGradient gradient(0,0, width,  height);
        gradient.setColorAt(0.0, QColor(0, 255, 0));
        gradient.setColorAt(0.5, QColor(255, 255, 0));
        gradient.setColorAt(1.0, QColor(255, 0, 0));*/

        //usage.setColor(QColor(190, 250, 255));
        usage.setColor(QColor(116, 151, 255));
        //usage.setBrush(gradient);
        usage.setWidthF(pen_width * 2);
        usage.setJoinStyle(Qt::RoundJoin);
        usage.setCapStyle(Qt::RoundCap);
    }
    painter->setPen(usage);

    painter->drawRect(inner);
 //   usage.setWidth(2);
 //   painter->setPen(usage);

    QPainterPath path;
    int max_value = -1;
    int prev_value = 0;
    int last_value = 0;
    qreal elapsed_step = qMin(m_elapsed_timer.elapsed(), (qint64)REQUEST_TIME) * 1.0 / REQUEST_TIME;

    {
        qreal x1, y1;
        const qreal x_step = (qreal)ow*1.0/(LIMIT - 2);
        const qreal x_step2 = x_step / 2;
        x1 = offset - (x_step * elapsed_step);
        qreal value(0.0);
        bool first(true);

        path.moveTo(x1, 0.0);
        QListIterator<int> iter(m_history);
        while (iter.hasNext()){
            int i_value = iter.next();
            prev_value = last_value;
            last_value = i_value;

            max_value = qMax(max_value, i_value);
            value = i_value / 100.0;
            if (i_value < 0)
                value = 0;
//            else if (i_value == 0)
//                value = .0001;
            if (first){ 
                path.lineTo(x1, value);
                first = false;
                y1 = value;
                continue;
            }

            path.cubicTo(x1 + x_step2, y1, x1 + x_step2, value, x1 + x_step, value);
            x1 += x_step;
            y1 = value;
        }
        path.lineTo(offset + ow, value);
        path.lineTo(offset + ow, 0.0);
    }

    if (stretch_y && max_value >= 0){
        QListIterator<int> step(m_steps);
        int value = 1;
        while (step.hasNext()){
            value = step.next();
            if (max_value <= value)
                break;
        }
        max_value = value;
        scale_factor = 100.0 / value;
    }

    
    QPainterPath result_path = scalePath(path, scale_factor, offset, oh);
    if (background_enabled){
        QLinearGradient gradient(width / 2, offset + oh, width / 2,  offset + (1.0 - scale_factor)*oh);
        gradient.setColorAt(0.0, QColor(0, 255, 0, 70));
        gradient.setColorAt(0.5, QColor(255, 255, 0, 70));
        gradient.setColorAt(1.0, QColor(255, 0, 0, 70));
        painter->setBrush(gradient);
        painter->drawPath(result_path);
    }
    else
        painter->strokePath(result_path, usage);
    painter->setClipping(false);


    QFont f;
    f.setStyleStrategy(QFont::ForceOutline);
    f.setPointSizeF(offset * 0.3);
    f.setStyleHint(QFont::Times);
    painter->setFont(f);
    if (stretch_y){
        painter->drawText(offset + ow, offset, QString::number(max_value >= 0 ? max_value : 100)+"%");
    }
    if (last_value >= 0){
        int x = offset + ow;
        qreal inter_value = ((last_value - prev_value)*elapsed_step + prev_value);
        int y = offset + oh * (1.0 - inter_value * 0.01 * scale_factor);
        painter->drawText(x, y, QString::number(qRound(inter_value))+"%");
    }
}
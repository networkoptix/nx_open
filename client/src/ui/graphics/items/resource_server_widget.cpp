#include "resource_server_widget.h"

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/video_server.h>

#include <ui/style/globals.h>

#define LIMIT 60
#define REQUEST_TIME 2000

QnResourceServerWidget::QnResourceServerWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /* = NULL */):
    QnResourceWidget(context, item, parent),
    m_alreadyUpdating(false),
    m_counter(0),
    m_redrawStatus(Qn::LoadingFrame){
        for (int i = 0; i < LIMIT; i++)
            m_history.append(-1);

        m_timer = new QTimer(this);
        connect(m_timer, SIGNAL(timeout()), this, SLOT(at_timer_update()));
        m_timer->start(REQUEST_TIME);

        m_background_gradient.setColorAt(0.0, qnGlobals->systemHealthColorBackground());
        m_background_gradient.setColorAt(1, QColor(0, 0, 0, 0));

}

void QnResourceServerWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
    assertPainters();

    switch (m_redrawStatus)
    {
    case Qn::NewFrame:
        setOverlayIcon(0, NO_ICON);
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
    server->apiConnection()->asyncGetStatistics(this, SLOT(at_statistics_received(int)));
}

void QnResourceServerWidget::at_statistics_received(int usage){
    m_alreadyUpdating = false;
    m_redrawStatus = Qn::NewFrame;
    m_history.push_back(usage);
    m_elapsed_timer.restart();
    m_counter++;
    if (m_history.count() > LIMIT)
        m_history.pop_front();
}

QPainterPath scalePath(const QPainterPath &source, qreal offset, qreal height){
    QPainterPath path;
    path.addPath(source);
    for (int i=0; i<path.elementCount(); ++i)  {
        const QPainterPath::Element &e = path.elementAt(i);
        qreal x = e.x;
        qreal y = offset + height * (1.0 - e.y);
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

    const bool grid_enabled = true;
    const bool gradient_fill = true;
    const bool background_circle = true;

    qreal offset = min / 20.0;
    qreal pen_width = width / 500.0;

    QRectF rect(0, 0, width, height);
    qreal oh = height - offset*2;
    qreal ow = width - offset*2;

    QRectF inner(offset, offset, ow, oh);

    painter->fillRect(rect, Qt::black);
    painter->setRenderHint(QPainter::Antialiasing);

    if (background_circle){
        qreal radius = min * 0.5;
        QPointF center(width * 0.5, height * 0.5);
        m_background_gradient.setCenter(center);
        m_background_gradient.setFocalPoint(center);
        m_background_gradient.setRadius(radius);
        painter->setBrush(m_background_gradient);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPointF(width/2, height/2), radius, radius);
    }

    QPen main_pen;{
        main_pen.setColor(qnGlobals->systemHealthColorMain());
        main_pen.setWidthF(pen_width * 2);
        main_pen.setJoinStyle(Qt::RoundJoin);
        main_pen.setCapStyle(Qt::RoundCap);
    }
    painter->setPen(main_pen);

    painter->drawRect(inner);
    painter->setClipRect(inner);

    const qreal x_step = (qreal)ow*1.0/(LIMIT - 2);
    const qreal x_step2 = x_step / 2;
    qreal elapsed_step = (qreal)qMin(m_elapsed_timer.elapsed(), (qint64)REQUEST_TIME) / (qreal)REQUEST_TIME;

    if (grid_enabled){
        QPen grid;
        grid.setColor(qnGlobals->systemHealthColorGrid());
        grid.setWidthF(pen_width);

        QPainterPath grid_path;
        for (qreal i = offset - (x_step * (elapsed_step + m_counter%4)); i < ow + offset; i += x_step*4){
            grid_path.moveTo(i, offset);
            grid_path.lineTo(i, oh + offset);
        }
        for (qreal i = x_step*2; i < oh; i += x_step*4){
            grid_path.moveTo(offset, i);
            grid_path.lineTo(ow + offset, i);
        }

        painter->strokePath(grid_path, grid);
    }

    QPainterPath path;
    int max_value = -1;
    int prev_value = 0;
    int last_value = 0;
    {
        qreal x1, y1;
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
    
    QPainterPath result_path = scalePath(path, offset, oh);
    if (gradient_fill){
        QLinearGradient gradient(width / 2, offset + oh, width / 2,  offset);
        gradient.setColorAt(0.0, qnGlobals->systemHealthColorGradientLow());
        gradient.setColorAt(0.5, qnGlobals->systemHealthColorGradientMid());
        gradient.setColorAt(1.0, qnGlobals->systemHealthColorGradientHigh());
        painter->setBrush(gradient);
        painter->drawPath(result_path);
    }
    else
        painter->strokePath(result_path, main_pen);
    painter->setClipping(false);

    QFont font;
    font.setStyleHint(QFont::SansSerif, QFont::ForceOutline);
    font.setPointSizeF(offset * 0.3);
    painter->setFont(font);
    {
        qreal inter_value = 0;
        if (last_value >= 0 && prev_value >= 0)
            inter_value = ((last_value - prev_value)*elapsed_step + prev_value);
        int x = offset*1.2 + ow;
        int y = offset + oh * (1.0 - inter_value * 0.01);
        painter->drawText(x, y, QString::number(qRound(inter_value))+"%");
    }
}
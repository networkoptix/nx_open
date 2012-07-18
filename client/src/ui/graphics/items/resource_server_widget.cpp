#include "resource_server_widget.h"

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/video_server.h>

#include <ui/style/globals.h>

#include <ui/graphics/items/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>

#define LIMIT 60
#define REQUEST_TIME 2000

QnResourceServerWidget::QnResourceServerWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /* = NULL */):
    QnResourceWidget(context, item, parent),
    m_alreadyUpdating(false),
    m_counter(0),
    m_model(tr("Unknown")),
    m_redrawStatus(Qn::LoadingFrame),
    m_backgroundGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0))
{
    for (int i = 0; i < LIMIT; i++)
        m_cpuUsageHistory.append(0);

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
    drawStatistics(rect(), painter);

    painter->beginNativePainting();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawOverlayIcon(0, rect());
    glDisable(GL_BLEND);
    painter->endNativePainting();

    if(m_footerOverlayWidget->isVisible() && !qFuzzyIsNull(m_footerOverlayWidget->opacity())) 
        emit updateOverlayTextLater();
}

void QnResourceServerWidget::updateOverlayText(){
    m_footerStatusLabel->setText(tr("CPU Model: %1").arg(m_model));
}

void QnResourceServerWidget::at_timer_update(){
    if (m_alreadyUpdating){
        if (m_redrawStatus != Qn::CannotLoad)
            m_redrawStatus = Qn::LoadingFrame;
    }
    m_alreadyUpdating = true;
    QnVideoServerResourcePtr server = resource().dynamicCast<QnVideoServerResource>();
    Q_ASSERT(server);
    server->apiConnection()->asyncGetStatistics(this, SLOT(at_statistics_received(int /* cpu usage */ , const QByteArray & /* model */, const QnHddUsageVector &/* hdd usage */)));
}

void QnResourceServerWidget::at_statistics_received(int cpuUsage, const QByteArray &model, const QnHddUsageVector &hddUsage){
    m_alreadyUpdating = false;
    m_redrawStatus = Qn::NewFrame;
    m_cpuUsageHistory.push_back(cpuUsage);

    for (int i = 0; i < hddUsage.size(); i++){
        if (m_hddUsageHistory.length() < i + 1){
            QList<int> hdd_next;
            for (int j = 0; j < LIMIT; j++)
                hdd_next.append(0);
            m_hddUsageHistory.append(hdd_next);
        }
        QList<int> *hdd = &(m_hddUsageHistory[i]);
        hdd->append(hddUsage[i]);
        if (hdd->count() > LIMIT)
            hdd->pop_front();
    }

    m_model = model;
    m_elapsed_timer.restart();
    m_counter++;
    if (m_cpuUsageHistory.count() > LIMIT)
        m_cpuUsageHistory.pop_front();
}

QPainterPath scalePath(const QPainterPath &source, qreal offset, qreal height, qreal dx){ // TODO: use QTransform instead
    QPainterPath path;
    path.addPath(source);
    for (int i=0; i<path.elementCount(); ++i)  {
        const QPainterPath::Element &e = path.elementAt(i);
        qreal x = e.x + dx;
        qreal y = offset + height * (1.0 - e.y);
        path.setElementPositionAt(i, x, y);
    }
    return path;
}

QPainterPath QnResourceServerWidget::createGraph(QList<int> *values, qreal x_step, int &prev_value, int &last_value){
    QPainterPath path;
    int max_value = -1;
    prev_value = 0;
    last_value = 0;
    const qreal x_step2 = x_step*.5;
    {
        qreal x1, y1;
        x1 = 0.0;
        qreal value(0.0);
        bool first(true);

        path.moveTo(x1, 0.0);
        QListIterator<int> iter(*values);
        while (iter.hasNext()){
            int i_value = iter.next();
            prev_value = last_value;
            last_value = i_value;

            max_value = qMax(max_value, i_value);
            value = i_value / 100.0;
            if (first){ 
                path.lineTo(x1, value);
                first = false;
                y1 = value;
                continue;
            }

            qreal y2 = value;

            if (y2 > y1){
                path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -90);
                path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, 90);
            } else if (y2 < y1){
                path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 90);
                path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -90);
            } else { // y2 == y1
                path.lineTo(x1 + x_step, y2);
            }
            x1 += x_step;
            y1 = value;
        }
    }
    return path;
}

void QnResourceServerWidget::drawStatistics(const QRectF &rect, QPainter *painter){
    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

    qreal width = rect.width();
    qreal height = rect.height();

   //painter->setOpacity(.5);
    qreal min = qMin(width, height);

    const bool grid_enabled = true;
    const bool background_circle = true;

    qreal offset = min / 20.0;
    qreal pen_width = width / 500.0;

    qreal oh = height - offset*2;
    qreal ow = width - offset*2;
    
    if (ow <= 0 || oh <= 0)
        return;

    QRectF inner(offset, offset, ow, oh); 

    painter->fillRect(rect, Qt::black);
    //painter->setRenderHint(QPainter::Antialiasing);

    if (background_circle){
        painter->beginNativePainting();
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glPushMatrix();
            glTranslatef(inner.center().x(), inner.center().y(), 1.0);
            qreal radius = min * 0.5 - offset;
            glScale(radius, radius);
            m_backgroundGradientPainter.paint(qnGlobals->backgroundGradientColor());
            glPopMatrix();

            glDisable(GL_BLEND);
        }
        painter->endNativePainting();
    }

    QPen main_pen;{
        main_pen.setColor(qnGlobals->systemHealthColorMain());
        main_pen.setWidthF(pen_width * 2);
  //      main_pen.setJoinStyle(Qt::RoundJoin);
  //      main_pen.setCapStyle(Qt::RoundCap);
    }
    painter->setPen(main_pen);

    painter->drawRect(inner);
    painter->setClipRect(inner); // TODO: it's better to avoid clipping.

    const qreal x_step = (qreal)ow*1.0/(LIMIT - 2);
    qreal elapsed_step = (qreal)qMin(m_elapsed_timer.elapsed(), (qint64)REQUEST_TIME) / (qreal)REQUEST_TIME;

    if (grid_enabled){
        QPen grid;
        QVector<QPoint> pointPairs;
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

    int prev_value = 0;
    int last_value = 0;
    QPainterPath path = createGraph(&m_cpuUsageHistory, x_step, prev_value, last_value);
    QPainterPath result_path = scalePath(path, offset, oh, offset - (x_step * elapsed_step));
    painter->strokePath(result_path, main_pen);

    for (int i = 0; i < m_hddUsageHistory.length(); i++){
        int hdd_prev_value = 0;
        int hdd_last_value = 0;
        QPainterPath hddpath = createGraph(&(m_hddUsageHistory[i]), x_step, hdd_prev_value, hdd_last_value);
        QPainterPath hdd_result_path = scalePath(hddpath, offset, oh, offset - (x_step * elapsed_step));
        main_pen.setColor(Qt::red);
        painter->strokePath(hdd_result_path, main_pen);
    }

    painter->setClipping(false);

    QFont font(this->font());
    font.setPointSizeF(offset * 0.3);
    painter->setFont(font);
    {
        qreal inter_value = 0;
        if (last_value >= 0 && prev_value >= 0)
            inter_value = ((last_value - prev_value)*elapsed_step + prev_value);
        int x = offset*1.1 + ow;
        int y = offset + oh * (1.0 - inter_value * 0.01);
        painter->drawText(x, y, QString::number(qRound(inter_value))+"%");
    }
}
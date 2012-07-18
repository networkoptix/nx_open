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
    m_redrawStatus(Qn::LoadingFrame),
    m_backgroundGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0))
{
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(at_timer_update()));
    m_timer->start(REQUEST_TIME);
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
    if (m_history.length() == 0)
        return;

    QnStatisticsHistoryData cpu = m_history[0];

    m_footerStatusLabel->setText(tr("CPU Model: %1").arg(cpu.first));
}

void QnResourceServerWidget::at_timer_update(){
    if (m_alreadyUpdating){
        if (m_redrawStatus != Qn::CannotLoad)
            m_redrawStatus = Qn::LoadingFrame;
    }
    m_alreadyUpdating = true;
    QnVideoServerResourcePtr server = resource().dynamicCast<QnVideoServerResource>();
    Q_ASSERT(server);
    server->apiConnection()->asyncGetStatistics(this, SLOT(at_statistics_received(const QnStatisticsDataVector &/* data */)));
}

QString getDataId(int id){
    if (id == 0)
        return "CPU";
    return QString("HDD%1").arg(id+1);
}

QColor getColorById(int id){
    switch(id){
        case 0: return qnGlobals->systemHealthColorMain();
        case 1: return Qt::red;
        case 2: return Qt::green;
    }
    return Qt::yellow;
}

void QnResourceServerWidget::at_statistics_received(const QnStatisticsDataVector &data){
    m_alreadyUpdating = false;
    m_redrawStatus = Qn::NewFrame;

    for (int i = 0; i < data.size(); i++){
        if (m_history.length() < i + 1){
            QList<int> dummy;
            for (int j = 0; j < LIMIT; j++)
                dummy.append(0);
            QnStatisticsHistoryData dummyItem(getDataId(i), dummy);
            m_history.append(dummyItem);
        }

        QnStatisticsHistoryData *item = &(m_history[i]);
        QnStatisticsData next_data = data[i];
        item->second.append(next_data.second);
        if (item->second.count() > LIMIT)
            item->second.pop_front();
    }

    m_elapsed_timer.restart();
    m_counter++;
}

QPainterPath QnResourceServerWidget::createGraph(QList<int> *values, qreal x_step, const qreal scale, int &prev_value, int &last_value){
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
                y1 = value * scale;
                path.lineTo(x1, y1);
                first = false;
                continue;
            }

            qreal y2 = value * scale;

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
            y1 = y2;
        }
    }
    return path;
}

void QnResourceServerWidget::drawStatistics(const QRectF &rect, QPainter *painter){
    
    qreal width = rect.width();
    qreal height = rect.height();

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

    /** Trying to get some cpu */
    // painter->setRenderHint(QPainter::Antialiasing); 

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

        /** Trying to get some cpu */
        //main_pen.setJoinStyle(Qt::RoundJoin);
        //main_pen.setCapStyle(Qt::RoundCap);
        main_pen.setJoinStyle(Qt::BevelJoin);
        main_pen.setCapStyle(Qt::FlatCap);
    }
    QnScopedPainterPenRollback penRollback(painter);
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
    qreal y_scale = -1.0 * oh;

    {
        QnScopedPainterTransformRollback transformRollback(painter);
        QTransform graphTransform = painter->transform();
        graphTransform.translate(offset - (x_step * elapsed_step), oh + offset);

        painter->setTransform(graphTransform);

        for (int i = 0; i < m_history.length(); i++){
            QPainterPath hddpath = createGraph(&(m_history[i].second), x_step, y_scale, prev_value, last_value);

            main_pen.setColor(getColorById(i));
            painter->strokePath(hddpath, main_pen);
        }
    }

    painter->setClipping(false);

    {
        QnScopedPainterFontRollback fontRollback(painter);
        QFont font(this->font());
        font.setPointSizeF(offset * 0.3);
        painter->setFont(font);
        {
            qreal inter_value = ((last_value - prev_value)*elapsed_step + prev_value);
            int x = offset*1.1 + ow;
            int y = offset + oh * (1.0 - inter_value * 0.01);
            painter->drawText(x, y, QString::number(qRound(inter_value))+"%");
        }

        /** Draw legend */
        {
            QnScopedPainterTransformRollback transformRollback(painter);
            QTransform legendTransform = painter->transform();
            QPainterPath legend;
            legend.lineTo(offset * .2, 0.0);
            legendTransform.translate(0.0, oh + offset * 0.5);
            painter->setTransform(legendTransform);
            for (int i = 0; i < m_history.length(); i++){
                main_pen.setColor(getColorById(i));
                painter->setPen(main_pen);
                painter->strokePath(legend, main_pen);
                painter->drawText(0.2 * offset, 0.0, m_history[i].first);
                legendTransform.translate(0.4 * offset, 0.0);
                painter->setTransform(legendTransform);
            }
        }
    }
    


}
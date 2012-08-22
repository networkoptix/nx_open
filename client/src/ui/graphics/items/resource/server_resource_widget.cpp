#include "server_resource_widget.h"

#include <utils/common/math.h> /* For M_PI. */

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/video_server_resource.h>

#include <api/video_server_connection.h>
#include <api/video_server_statistics_manager.h>
#include <api/video_server_statistics_data.h>

#include <ui/style/globals.h>

#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/painters/radial_gradient_painter.h>

#include <QtCore/QHash>

/** How many points of the chart are shown on the screen simultaneously */
#define CHART_POINTS_LIMIT 60

/** Data update period. For the best result should be equal to server's */
#define REQUEST_TIME 2000

namespace {
    /** Convert angle from radians to degrees */
    qreal radiansToDegrees(qreal radian) {
        return (180 * radian) / M_PI;
    }

    /** Get corresponding color from config */
    QColor getColorById(int id) {
        QnColorVector colors = qnGlobals->systemHealthColors();
        return colors[id % colors.size()];
    }

    /** Create path for the chart */
    QPainterPath createChartPath(const QnStatisticsData *values, qreal x_step, qreal scale, qreal elapsed_step, qreal *current_value) {
        QPainterPath path;
        int max_value = -1;
        int prev_value = 0;
        int last_value = 0;
        const qreal x_step2 = x_step*.5;
        
        qreal x1, y1;
        x1 = -x_step * elapsed_step;

        qreal angle = elapsed_step >= 0.5 
            ? radiansToDegrees(qAcos(2 * (1 - elapsed_step))) 
            : radiansToDegrees(qAcos(2 * elapsed_step));

        bool first(true);

        QnStatisticsDataIterator iter(*values);
        bool last = !iter.hasNext();
        while (!last){
            int i_value = iter.next();
            last = !iter.hasNext();

            prev_value = last_value;
            last_value = i_value;

            max_value = qMax(max_value, i_value);
            if (first){ 
                y1 = i_value * scale * 0.01;
                path.moveTo(x1, y1);
                first = false;
                continue;
            }

            qreal y2 = i_value * scale * 0.01;

            /* Note that we're using 89 degrees for arc length for a reason.
             * When using full 90 degrees, arcs are connected, but Qt still
             * inserts an empty line segment to join consecutive arcs. 
             * Path triangulator then chokes when trying to calculate a normal
             * for this line segment, producing NaNs in its output.
             * These NaNs are then fed to GPU, resulting in artifacts. */

            /* Drawing only second part of the arc, cut at the beginning */
            if (x1 + x_step2 < 0.0) {
                if (y2 > y1) {
                    path.arcMoveTo(x1 + x_step2, y1, x_step, (y2 - y1), 180 + angle);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180 + angle, 90 - angle);
                } else if (y2 < y1) {
                    path.arcMoveTo(x1 + x_step2, y2, x_step, (y1 - y2), 180 - angle);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180 - angle, -90 + angle);
                } else { // y2 == y1
                    path.moveTo(0.0, y1);
                    path.lineTo(x1 + x_step, y2);
                }
            }
            /* Drawing both parts of the arc, cut at the beginning */
            else if (x1 < 0.0) {
                if (y2 > y1) {
                    path.arcMoveTo(x1 - x_step2, y1, x_step, (y2 - y1), angle);
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), angle, -angle);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, 89);
                } else if (y2 < y1) {
                    path.arcMoveTo(x1 - x_step2, y2, x_step, (y1 - y2), -angle);
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -angle, angle);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -89);
                } else { // y2 == y1
                    path.moveTo(0.0, y1);
                    path.lineTo(x1 + x_step, y2);
                }
            }
            /* Drawing both parts of the arc as usual */
            else if (!last) { 
                if (y2 > y1) {
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -89);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, 89);
                } else if (y2 < y1) {
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 89);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -89);
                } else { // y2 == y1
                    path.lineTo(x1 + x_step, y2);
                }
            }
            /* Drawing both parts of the arc, cut at the end */
            else if (elapsed_step >= 0.5) {
                if (y2 > y1) {
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -89);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, angle);
                } else if (y2 < y1) {
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 89);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -angle);
                } else { // y2 == y1
                    path.lineTo(x1 + x_step * elapsed_step, y2);
                }
            } 
            /* Drawing only first part of the arc, cut at the end */
            else {
                if (y2 > y1) {
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -90 + angle);
                } else if (y2 < y1) {
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 90 - angle);
                } else { // y2 == y1
                    path.lineTo(x1 + x_step * elapsed_step, y2);
                }
            }
            x1 += x_step;
            y1 = y2;
        }
        /** value that will be shown */
        *current_value = ((last_value - prev_value)*elapsed_step + prev_value);
        return path;
    }

    class QnBackgroundGradientPainterFactory {
    public:
        QnRadialGradientPainter *operator()(const QGLContext *context) {
            return new QnRadialGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0), context);
        }
    };

    typedef QnGlContextData<QnRadialGradientPainter, QnBackgroundGradientPainterFactory> QnBackgroundGradientPainterStorage;
    Q_GLOBAL_STATIC(QnBackgroundGradientPainterStorage, qn_serverResourceWidget_backgroundGradientPainterStorage)

} // anonymous namespace

QnServerResourceWidget::QnServerResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /* = NULL */):
    QnResourceWidget(context, item, parent),
    m_lastHistoryId(-1),
    m_counter(0),
    m_renderStatus(Qn::NothingRendered)
{
    m_resource = base_type::resource().dynamicCast<QnVideoServerResource>();
    if(!m_resource) 
        qnCritical("Server resource widget was created with a non-server resource.");

    m_statisticsId = videoServerStatisticsManager()->registerServerWidget(m_resource, this, SLOT(at_statistics_received()));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    timer->start(REQUEST_TIME);

    /* Run handlers. */
    updateButtonsVisibility();
}

QnServerResourceWidget::~QnServerResourceWidget() {
    videoServerStatisticsManager()->unRegisterServerWidget(m_resource, m_statisticsId, this);
    ensureAboutToBeDestroyedEmitted();
}

QnVideoServerResourcePtr QnServerResourceWidget::resource() const {
    return m_resource;
}

Qn::RenderStatus QnServerResourceWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &rect) {
    Q_UNUSED(channel);

    drawStatistics(rect, painter);

    return m_renderStatus;
}

void QnServerResourceWidget::drawStatistics(const QRectF &rect, QPainter *painter) {
    qreal width = rect.width();
    qreal height = rect.height();

    qreal min = qMin(width, height);

    qreal offset = min / 20.0;
    qreal pen_width = width / 500.0;

    qreal oh = height - offset*2;
    qreal ow = width - offset*2;

    if (ow <= 0 || oh <= 0)
        return;

    QRectF inner(offset, offset, ow, oh); 

    /** Draw background */
    if(!m_backgroundGradientPainter)
        m_backgroundGradientPainter = qn_serverResourceWidget_backgroundGradientPainterStorage()->get(QGLContext::currentContext());

    painter->beginNativePainting();
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glColor4f(0.0, 0.0, 0.0, 1.0);
        glBegin(GL_QUADS);
        glVertices(rect);
        glEnd();

        glPushMatrix();
        glTranslatef(inner.center().x(), inner.center().y(), 1.0);
        qreal radius = min * 0.5 - offset;
        glScale(radius, radius);
        m_backgroundGradientPainter->paint(qnGlobals->backgroundGradientColor());
        glPopMatrix();

        glDisable(GL_BLEND);
    }
    painter->endNativePainting();

    const qreal x_step = (qreal)ow*1.0/(CHART_POINTS_LIMIT - 2);

    qreal elapsed_step = m_renderStatus == Qn::CannotRender ? 0 :
            (qreal)qBound((qreal)0, (qreal)m_elapsedTimer.elapsed(), (qreal)REQUEST_TIME) / (qreal)REQUEST_TIME;

    qDebug() << "elapsed step" << elapsed_step;

    /** Draw grid */
    {
        QPen grid;
        grid.setColor(qnGlobals->systemHealthColorGrid());
        grid.setWidthF(pen_width);

        QPainterPath grid_path;
        for (qreal i = offset - (x_step * (elapsed_step + m_counter%4 - 4)); i < ow + offset; i += x_step*4){
            grid_path.moveTo(i, offset);
            grid_path.lineTo(i, oh + offset);
        }
        for (qreal i = x_step*2 + offset; i < oh; i += x_step*4){
            grid_path.moveTo(offset, i);
            grid_path.lineTo(ow + offset, i);
        }
        painter->strokePath(grid_path, grid);
    }

    QList<qreal> values;

    /** Draw graph lines */
    {
        QnScopedPainterTransformRollback transformRollback(painter);
        Q_UNUSED(transformRollback)

        QTransform graphTransform = painter->transform();
        graphTransform.translate(offset, oh + offset);
        painter->setTransform(graphTransform);

        QPen graphPen;
        graphPen.setWidthF(pen_width * 2);
        graphPen.setCapStyle(Qt::FlatCap);

        QnStatisticsIterator iter(m_history);
        int counter = 0;
        while (iter.hasNext()){
            iter.next();
            qreal current_value = 0;
            QPainterPath path = createChartPath(iter.value(), x_step, -1.0 * oh, elapsed_step, &current_value);
            values.append(current_value);

            graphPen.setColor(getColorById(counter++));
            painter->strokePath(path, graphPen);
        }
    }

    /** Draw frame and legend */
    {
        QnScopedPainterPenRollback penRollback(painter);
        Q_UNUSED(penRollback)

        QPen main_pen;
        main_pen.setColor(getColorById(0));
        main_pen.setWidthF(pen_width * 2);
        main_pen.setJoinStyle(Qt::MiterJoin);

        painter->setPen(main_pen);
        painter->drawRect(inner);

        QnScopedPainterFontRollback fontRollback(painter);
        Q_UNUSED(fontRollback)

        QFont font(this->font());

#ifndef Q_WS_X11
        font.setPointSizeF(offset * 0.3);
        painter->setFont(font);
        /** Draw text values */
        {
            qreal x = offset*1.1 + ow;
            for (int i = 0; i < values.length(); i++){
                qreal inter_value = values[i];
                main_pen.setColor(getColorById(i));
                painter->setPen(main_pen);
                qreal y = offset + oh * (1.0 - inter_value * 0.01);
                painter->drawText(x, y, QString::number(qRound(inter_value))+QLatin1Char('%'));
            }
        }

        /** Draw legend */
        {
            QnScopedPainterTransformRollback transformRollback(painter);
            Q_UNUSED(transformRollback)

            QTransform legendTransform = painter->transform();
            QPainterPath legend;

            legend.addRect(0.0, 0.0, -offset*0.2, -offset*0.2);

            legendTransform.translate(width * 0.5 - offset * m_history.length(), oh + offset * 1.5);
            painter->setTransform(legendTransform);
            int counter = 0;
            QnStatisticsIterator iter(m_history);
            while (iter.hasNext()){
                iter.next();
                main_pen.setColor(getColorById(counter++));
                painter->setPen(main_pen);
                painter->strokePath(legend, main_pen);
                painter->drawText(offset*0.1, offset*0.1, iter.key());
                legendTransform.translate(offset * 2, 0.0);
                painter->setTransform(legendTransform);
            }
        }
#else //Q_WS_X11
        font.setPixelSize(20);
        painter->setFont(font);
        /** Draw text values */
        {
            qreal c = offset*0.02;
            painter->scale(c, c);
            c = 1/c;
            qreal x = offset*1.1 + ow;
            for (int i = 0; i < values.length(); i++){
                qreal inter_value = values[i];
                main_pen.setColor(getColorById(i));
                painter->setPen(main_pen);
                qreal y = offset + oh * (1.0 - inter_value * 0.01);
                painter->drawText(x*c, y*c, QString::number(qRound(inter_value))+QLatin1Char('%'));
            }
            painter->scale(c, c);
        }

        /** Draw legend */
        {
            QnScopedPainterTransformRollback transformRollback(painter);
            Q_UNUSED(transformRollback)

            painter->translate(width * 0.5 - offset * m_history.size(), oh + offset * 1.5);
            qreal c = offset*0.02;
            painter->scale(c, c);
            c = 1/c;

            QPainterPath legend;
            legend.addRect(0.0, 0.0, -offset*0.2*c, -offset*0.2*c);
            main_pen.setWidthF(pen_width * 2 * c);

            int counter = 0;
            QnStatisticsIterator iter(m_history);
            while (iter.hasNext()){
                iter.next();
                main_pen.setColor(getColorById(counter++));
                painter->setPen(main_pen);
                painter->strokePath(legend, main_pen);
                painter->drawText(offset*0.1*c, offset*0.1*c, iter.key());
                painter->translate(offset * 2*c, 0.0);
            }
            painter->scale(c, c);
        }
    #endif //Q_WS_X11
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QnResourceWidget::Buttons QnServerResourceWidget::calculateButtonsVisibility() const {
    return base_type::calculateButtonsVisibility() & (CloseButton | RotateButton);
}

void QnServerResourceWidget::at_timer_timeout() {
    videoServerStatisticsManager()->notifyTimer(m_resource, m_statisticsId);
}

void QnServerResourceWidget::at_statistics_received(){
    qDebug() << "widget" << m_statisticsId << "received statistics";

    QnStatisticsHistory *history_update = new QnStatisticsHistory();
    qint64 id = videoServerStatisticsManager()->getHistory(m_resource, m_lastHistoryId, history_update);
    if (id < 0){
        m_renderStatus = Qn::CannotRender;
        return;
    }

    if (id == m_lastHistoryId){
        m_renderStatus = Qn::OldFrameRendered;
        return;
    }

    // remove charts that are no exist anymore
    QnStatisticsCleaner cleaner(m_history);
    while (cleaner.hasNext()) {
         cleaner.next();
         if (!(history_update->contains(cleaner.key())))
            cleaner.remove();
    }

    // update existsing charts
    QnStatisticsIterator updater(*history_update);
    while (updater.hasNext()) {
         updater.next();
         updateValues(updater.key(), updater.value(), id);
    }

    delete history_update;

    m_lastHistoryId = id;
    m_renderStatus = Qn::NewFrameRendered;

    m_elapsedTimer.restart();
    m_counter++;
}

void QnServerResourceWidget::updateValues(QString key, QnStatisticsData *newValues, qint64 newId){
    QnStatisticsData *oldValues;

    if (!m_history.contains(key)){
        oldValues = new QnStatisticsData();
        for (int i = 0; i < CHART_POINTS_LIMIT; i++)
            oldValues->append(0);
        m_history[key] = oldValues;
    } else {
        oldValues = m_history[key];
    }

    // minimal stored id
    qint64 minId = qMin(newId - CHART_POINTS_LIMIT + 1, qint64(0));

    if (m_lastHistoryId < minId){
        oldValues->clear();
    } else {
        int size = oldValues->size();
        for (int i = 1; i < size - (m_lastHistoryId - minId); i++)
            oldValues->removeFirst();
    }

    while (!newValues->isEmpty())
        oldValues->append(newValues->takeFirst());
}

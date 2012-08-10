#include "server_resource_widget.h"

#include <utils/common/math.h> /* For M_PI. */

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/video_server_resource.h>
#include <api/video_server_connection.h>

#include <ui/style/globals.h>

#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/painters/radial_gradient_painter.h>

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
    QPainterPath createChartPath(QList<int> *values, qreal x_step, const qreal scale, qreal &current_value, const qreal elapsed_step) {
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

        QListIterator<int> iter(*values);
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

            /** Drawing only second part of the arc, cut at the beginning */
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
            /** Drawing both part of the arc, cut at the beginning */
            else if (x1 < 0.0) {
                if (y2 > y1) {
                    path.arcMoveTo(x1 - x_step2, y1, x_step, (y2 - y1), angle);
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), angle, -angle);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, 90);
                } else if (y2 < y1) {
                    path.arcMoveTo(x1 - x_step2, y2, x_step, (y1 - y2), -angle);
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -angle, angle);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -90);
                } else { // y2 == y1
                    path.moveTo(0.0, y1);
                    path.lineTo(x1 + x_step, y2);
                }
            }
            /** Drawing both part of the arc as usual */
            else if (!last) { 
                if (y2 > y1) {
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -90);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, 90);
                } else if (y2 < y1) {
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 90);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -90);
                } else { // y2 == y1
                    path.lineTo(x1 + x_step, y2);
                }
            }
            /** Drawing both part of the arc, cut at the end */
            else if (elapsed_step >= 0.5) {
                if (y2 > y1) {
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -90);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, angle);
                } else if (y2 < y1) {
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 90);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -angle);
                } else { // y2 == y1
                    path.lineTo(x1 + x_step * elapsed_step, y2);
                }
            } 
            /** Drawing only first part of the arc, cut at the end */
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
        current_value = ((last_value - prev_value)*elapsed_step + prev_value);
        return path;
    }

    class QnBackgroundGradientPainterFactory {
    public:
        QnRadialGradientPainter *operator()(const QGLContext *context) {
            return new QnRadialGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0), context);
        }
    };

    typedef QnGlContextData<QnRadialGradientPainter, QnBackgroundGradientPainterFactory> QnBackgroundGradientPainterStorage;
    Q_GLOBAL_STATIC(QnBackgroundGradientPainterStorage, qn_serverResourceWidget_backgroundGradientPainterStorage);

} // anonymous namespace

QnServerResourceWidget::QnStatisticsHistoryData::QnStatisticsHistoryData(QString id, QString description)
{
    this->id = id;
    this->description = description;
    for (int i = 0; i < CHART_POINTS_LIMIT; i++)
        history.append(0);
}

void QnServerResourceWidget::QnStatisticsHistoryData::append(int value) {
    history.append(value);
    if (history.count() > CHART_POINTS_LIMIT)
        history.pop_front();
}

QnServerResourceWidget::QnServerResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /* = NULL */):
    QnResourceWidget(context, item, parent),
    m_alreadyUpdating(false),
    m_counter(0),
    m_renderStatus(Qn::NothingRendered)
{
    m_resource = base_type::resource().dynamicCast<QnVideoServerResource>();
    if(!m_resource) 
        qnCritical("Server resource widget was created with a non-server resource.");

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    timer->start(REQUEST_TIME);

    /* Run handlers. */
    updateButtonsVisibility();
}

QnServerResourceWidget::~QnServerResourceWidget() {
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
    qreal elapsed_step = (qreal)qMin(m_elapsedTimer.elapsed(), (qint64)REQUEST_TIME) / (qreal)REQUEST_TIME;

    /** Draw grid */
    {
        QPen grid;
        QVector<QPoint> pointPairs;
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
        QTransform graphTransform = painter->transform();
        graphTransform.translate(offset, oh + offset);
        painter->setTransform(graphTransform);

        QPen graphPen;
        graphPen.setWidthF(pen_width * 2);
        graphPen.setJoinStyle(Qt::RoundJoin);
        graphPen.setCapStyle(Qt::RoundCap);

        for (int i = 0; i < m_history.length(); i++){
            qreal current_value = 0;
            QPainterPath path = createChartPath(&(m_history[i].history), x_step, -1.0 * oh, current_value, elapsed_step);
            values.append(current_value);

            graphPen.setColor(getColorById(i));
            painter->strokePath(path, graphPen);
        }
    }

    /** Draw frame and legend */
    {
        QnScopedPainterPenRollback penRollback(painter);
        QPen main_pen;{
            main_pen.setColor(getColorById(0));
            main_pen.setWidthF(pen_width * 2);
            main_pen.setJoinStyle(Qt::RoundJoin);
            main_pen.setCapStyle(Qt::RoundCap);
        }
        painter->setPen(main_pen);
        painter->drawRect(inner);

        QnScopedPainterFontRollback fontRollback(painter);
        QFont font(this->font());
        font.setPointSizeF(offset * 0.3);
        painter->setFont(font);

        /** Draw text values */
        {
            int x = offset*1.1 + ow;
            for (int i = 0; i < values.length(); i++){
                qreal inter_value = values[i];
                main_pen.setColor(getColorById(i));
                painter->setPen(main_pen);
                int y = offset + oh * (1.0 - inter_value * 0.01);
                painter->drawText(x, y, QString::number(qRound(inter_value))+QLatin1Char('%'));
            }
        }

        /** Draw legend */
        {
            QnScopedPainterTransformRollback transformRollback(painter);
            QTransform legendTransform = painter->transform();
            QPainterPath legend;

            legend.addRect(0.0, 0.0, -offset*0.2, -offset*0.2);

            legendTransform.translate(width * 0.5 - offset * m_history.length(), oh + offset * 1.5);
            painter->setTransform(legendTransform);
            for (int i = 0; i < m_history.length(); i++){
                main_pen.setColor(getColorById(i));
                painter->setPen(main_pen);
                painter->strokePath(legend, main_pen);
                painter->drawText(offset*0.1, offset*0.1, m_history[i].id);
                legendTransform.translate(offset * 2, 0.0);
                painter->setTransform(legendTransform);
            }
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QnResourceWidget::Buttons QnServerResourceWidget::calculateButtonsVisibility() const {
    return base_type::calculateButtonsVisibility() & (CloseButton | RotateButton);
}

void QnServerResourceWidget::at_timer_timeout() {
    if (m_alreadyUpdating) {
        if (m_renderStatus != Qn::CannotRender)
            m_renderStatus = Qn::OldFrameRendered;
    }

    m_alreadyUpdating = true;
    m_resource->apiConnection()->asyncGetStatistics(this, SLOT(at_statisticsReceived(const QnStatisticsDataVector &)));
}

void QnServerResourceWidget::at_statisticsReceived(const QnStatisticsDataVector &data) {
    m_alreadyUpdating = false;
    m_renderStatus = Qn::NewFrameRendered;

    int cpuCounter = 0;
    int hddCounter = 0;
    
    for (int i = 0; i < data.size(); i++) {
        QnStatisticsData next_data = data[i];

        if (m_history.length() < i + 1) {
            QString id = next_data.device == QnStatisticsData::CPU 
                ? tr("CPU") + QString::number(cpuCounter++)
                : tr("HDD") + QString::number(hddCounter++);
            m_history.append(QnStatisticsHistoryData(id, next_data.description));
        }

        QnStatisticsHistoryData *item = &(m_history[i]);
        item->append(next_data.value);
    }

    m_elapsedTimer.restart();
    m_counter++;
}

#include "server_resource_widget.h"

#include <utils/common/math.h> /* For M_PI. */

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/media_server_resource.h>

#include <api/media_server_connection.h>
#include <api/media_server_statistics_manager.h>
#include <api/media_server_statistics_data.h>

#include <ui/style/globals.h>
#include <ui/help/help_topics.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/painters/radial_gradient_painter.h>
#include <ui/workbench/workbench_context.h>

/** Data update period. For the best result should be equal to mediaServerStatisticsManager's */
#define REQUEST_TIME 2000 // TODO: #GDM extract this one from server's response (updatePeriod element).

namespace {
    /** Convert angle from radians to degrees */
    qreal radiansToDegrees(qreal radian) {
        return (180 * radian) / M_PI;
    }

    /** Get corresponding color from config */
    QColor getColorByKey(const QString &key) {
        int id;
        // TODO: #gdm
        // It seems that qHash is not needed here and actually harmful here.
        // Why don't we just use plain numbering?
        // CPU -> 0
        // RAM -> 1
        // C: -> 2
        // D: -> 3
        // E: -> 4
        // ...
        // 
        // And for linux:
        // hda -> 2
        // hdb -> 3
        // ...
        // 
        // This way we won't have collisions for sure.

        // some hacks to align hashes for keys like 'C:' and 'sda'
        // strongly depends on size of systemHealthColors
        QLatin1String salt = QLatin1String("2");
        if (key == QLatin1String("CPU"))
            id = 7;
        else if (key == QLatin1String("RAM"))
            id = 8;
        else
        if (key.contains(QLatin1Char(':'))) {
            // cutting keys like 'C:' to 'C'. Also works with complex keys such as 'C: E:'
            QString key2 = key.at(0);
            id = qHash(salt + key2);
        }
        else
            // linux hdd keys
            id = qHash(salt + key);
        QnColorVector colors = qnGlobals->systemHealthColors();
        return colors[id % colors.size()];
    }



    /** Create path for the chart */
    QPainterPath createChartPath(const QnStatisticsData values, qreal x_step, qreal scale, qreal elapsedStep, qreal *currentValue) {
        QPainterPath path;
        qreal maxValue = -1;
        //qreal value;
        qreal lastValue = 0;
        const qreal x_step2 = x_step*.5;
        
        qreal x1, y1;
        x1 = -x_step * elapsedStep;

        qreal angle = elapsedStep >= 0.5 
            ? radiansToDegrees(qAcos(2 * (1 - elapsedStep))) 
            : radiansToDegrees(qAcos(2 * elapsedStep));

        bool first(true);
        bool last = false;
        *currentValue = -1;
        for(QLinkedList<qreal>::const_iterator pos = values.values.begin(); pos != values.values.end(); pos++) {
            qreal value = qMin(*pos, 1.0);
            //bool noData = value < 0;
            value = qMax(value, -0.005);
            last = std::next(pos) == values.values.end();
            maxValue = qMax(maxValue, value);
            if (first) {
                y1 = value * scale;
                path.moveTo(x1, y1);
                first = false;
                continue;
            }

            qreal y2 = value * scale;


            /* Note that we're using 89 degrees for arc length for a reason.
             * When using full 90 degrees, arcs are connected, but Qt still
             * inserts an empty line segment to join consecutive arcs. 
             * Path triangulator then chokes when trying to calculate a normal
             * for this line segment, producing NaNs in its output.
             * These NaNs are then fed to GPU, resulting in artifacts. */

            // TODO: #gdm This logic seems overly complicated to me.
            //            I'm sure we can do the same with a code that is at least three times shorter.

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
            else if (elapsedStep >= 0.5) {
                if (y2 > y1) {
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -89);
                    path.arcTo(x1 + x_step2, y1, x_step, (y2 - y1), 180, angle);
                } else if (y2 < y1) {
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 89);
                    path.arcTo(x1 + x_step2, y2, x_step, (y1 - y2), 180, -angle);
                } else { // y2 == y1
                    path.lineTo(x1 + x_step * elapsedStep, y2);
                }
            } 
            /* Drawing only first part of the arc, cut at the end */
            else {
                if (y2 > y1) {
                    path.arcTo(x1 - x_step2, y1, x_step, (y2 - y1), 90, -90 + angle);
                } else if (y2 < y1) {
                    path.arcTo(x1 - x_step2, y2, x_step, (y1 - y2), -90, 90 - angle);
                } else { // y2 == y1
                    path.lineTo(x1 + x_step * elapsedStep, y2);
                }
            }
            
            if(last) {
                /* Value that will be shown */
                *currentValue = (lastValue * (1.0 - elapsedStep) + value * elapsedStep);
                break;
            }

            x1 += x_step;
            y1 = y2;
            lastValue = value;
        }

        return path;
    }

    bool statisticsDataLess(const QnStatisticsData &first, const QnStatisticsData &second) {
        if (first.deviceType != second.deviceType)
            return first.deviceType < second.deviceType;
        return first.description.toLower() < second.description.toLower();
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
    m_manager(context->instance<QnMediaServerStatisticsManager>()),
    m_lastHistoryId(-1),
    m_counter(0),
    m_renderStatus(Qn::NothingRendered)
{
    m_resource = base_type::resource().dynamicCast<QnMediaServerResource>();
    if(!m_resource) 
        qnCritical("Server resource widget was created with a non-server resource.");

    m_storageLimit = m_manager->storageLimit();
    m_manager->registerServerWidget(m_resource, this, SLOT(at_statistics_received()));

    /* Note that this slot is already connected to nameChanged signal in 
     * base class's constructor.*/
    connect(m_resource.data(), SIGNAL(urlChanged(const QnResourcePtr &)), this, SLOT(updateTitleText()));

    /* Run handlers. */
    updateButtonsVisibility();
    updateTitleText();
    at_statistics_received();
}

QnServerResourceWidget::~QnServerResourceWidget() {
    m_manager->unregisterServerWidget(m_resource, this);

    ensureAboutToBeDestroyedEmitted();
}

QnMediaServerResourcePtr QnServerResourceWidget::resource() const {
    return m_resource;
}

int QnServerResourceWidget::helpTopicAt(const QPointF &pos) const {
    Q_UNUSED(pos)
    return Qn::MainWindow_MonitoringItem_Help;
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

        glColor4f(0.0, 0.0, 0.0, painter->opacity());
        glBegin(GL_QUADS);
        glVertices(rect);
        glEnd();

        glPushMatrix();
        glTranslatef(inner.center().x(), inner.center().y(), 1.0);
        qreal radius = min * 0.5 - offset;
        glScale(radius, radius);
        m_backgroundGradientPainter->paint(toTransparent(qnGlobals->backgroundGradientColor(), painter->opacity()));
        glPopMatrix();

        glDisable(GL_BLEND);
    }
    painter->endNativePainting();


    qreal elapsed_step = m_renderStatus == Qn::CannotRender ? 0 :
            (qreal)qBound((qreal)0, (qreal)m_elapsedTimer.elapsed(), (qreal)REQUEST_TIME) / (qreal)REQUEST_TIME;

    const qreal x_step = (qreal)ow*1.0/(m_storageLimit - 2);
    const qreal y_step = oh * 0.025;

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
        for (qreal i = y_step*4 + offset; i < oh + offset; i += y_step*4){
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

        qreal space_offset = pen_width * 2;

        QTransform graphTransform = painter->transform();
        graphTransform.translate(offset, oh + offset - space_offset);
        painter->setTransform(graphTransform);

        QPen graphPen;
        graphPen.setWidthF(pen_width * 2);
        graphPen.setCapStyle(Qt::FlatCap);

        foreach(QString key, m_sortedKeys) {
            QnStatisticsData &stats = m_history[key];
            qreal currentValue = 0;
            QPainterPath path = createChartPath(stats, x_step, -1.0 * (oh - space_offset*2), elapsed_step, &currentValue);
            values.append(currentValue);
            graphPen.setColor(getColorByKey(key));
            painter->strokePath(path, graphPen);
        }
    }

    /** Draw frame and legend */
    {
        QnScopedPainterPenRollback penRollback(painter);
        Q_UNUSED(penRollback)

        QPen main_pen;
        main_pen.setColor(getColorByKey(QLatin1String("CPU")));
        main_pen.setWidthF(pen_width * 2);
        main_pen.setJoinStyle(Qt::MiterJoin);

        painter->setPen(main_pen);
        painter->drawRect(inner);

        QnScopedPainterFontRollback fontRollback(painter);
        Q_UNUSED(fontRollback)
        QFont font(this->font());

#ifdef Q_OS_LINUX
        // DO NOT MODIFY THIS. To change font size, modify 'zoomCoef' variable below.
        int fontSize = 20;
#else
        // DO NOT MODIFY THIS. To change font size, modify 'zoomCoef' variable below.
        int fontSize = 80;
#endif
        font.setPixelSize(fontSize);
        painter->setFont(font);

        /* Draw text values on the right side */
        {
            // modify this if you want to change font size
            qreal zoomCoef = 0.4 * offset;

            qreal zoom(zoomCoef / fontSize);
            qreal unzoom(fontSize / zoomCoef);

            painter->scale(zoom, zoom);
            qreal x = unzoom * (offset * 1.1 + ow);
            for (int i = 0; i < values.length(); i++){
                qreal interValue = values[i];
                main_pen.setColor(getColorByKey(m_sortedKeys.at(i)));
                painter->setPen(main_pen);
                qreal y = unzoom * (offset + oh * (1.0 - interValue));
                if (interValue >= 0)
                    painter->drawText(x, y, tr("%1%").arg(qRound(interValue * 100.0)));
            }
            painter->scale(unzoom, unzoom);
        }

        /* Draw legend */
        {
            // modify this if you want to change squares size
            qreal squareSize = 0.2 * offset;

            // modify this if you want to change font size
            qreal zoomCoef = 0.5 * offset;

            int space = offset; // space for the square drawing and between legend elements

            qreal zoom(zoomCoef / fontSize);
            qreal unzoom(fontSize / zoomCoef);

            QnScopedPainterTransformRollback transformRollback(painter);
            Q_UNUSED(transformRollback)

            // move to the center point
            painter->translate(width * 0.5, oh + offset * 1.65);

            painter->scale(zoom, zoom);

            qreal legendOffset = 0.0;
            foreach(QString key, m_sortedKeys)
                legendOffset += painter->fontMetrics().width(key) + space * unzoom;

            // move left to half of the total legend width
            painter->translate(-legendOffset * 0.5, 0.0);

            QPainterPath legend;
            legend.addRect(0.0, 0.0, -squareSize * unzoom, -squareSize * unzoom);

            QPen legendPen(main_pen);
            legendPen.setWidthF(pen_width * 2 * unzoom);

            foreach(QString key, m_sortedKeys) {
                legendPen.setColor(getColorByKey(key));
                painter->setPen(legendPen);
                painter->strokePath(legend, legendPen);
                painter->drawText(offset * 0.1* unzoom, offset * 0.1* unzoom, key);
                painter->translate(painter->fontMetrics().width(key) + space * unzoom, 0.0);
            }
            painter->scale(unzoom, unzoom);
        }
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QString QnServerResourceWidget::calculateTitleText() const {
    return tr("%1 (%2)").arg(m_resource->getName()).arg(QUrl(m_resource->getUrl()).host());
}

QnResourceWidget::Buttons QnServerResourceWidget::calculateButtonsVisibility() const {
    return base_type::calculateButtonsVisibility() & (CloseButton | RotateButton);
}

void QnServerResourceWidget::at_statistics_received() {
    qint64 id = m_manager->historyId(m_resource);
    if (id < 0) {
        m_renderStatus = Qn::CannotRender;
        return;
    }

    if (id == m_lastHistoryId) {
        m_renderStatus = Qn::OldFrameRendered;
        return;
    }

    m_history = m_manager->history(m_resource);
    bool reSort = true;
    if (m_history.keys().size() == m_sortedKeys.size()) {
        foreach (QString key, m_sortedKeys)
            if (!m_history.contains(key))
                break;
        reSort = false;
    }

    if (reSort) {
        m_sortedKeys.clear();

        QList<QnStatisticsData> tmp(m_history.values());
        qSort(tmp.begin(), tmp.end(), statisticsDataLess);
        foreach(QnStatisticsData key, tmp)
            m_sortedKeys.append(key.description);
    }

    m_lastHistoryId = id;
    m_renderStatus = Qn::NewFrameRendered;

    m_elapsedTimer.restart();
    m_counter++;
}

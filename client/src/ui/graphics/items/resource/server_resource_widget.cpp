#include "server_resource_widget.h"

#include <iterator> /* For std::advance. */

#include <utils/math/math.h> /* For M_PI. */
#include <utils/math/color_transformations.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/media_server_resource.h>

#include <api/media_server_connection.h>
#include <api/media_server_statistics_manager.h>
#include <api/media_server_statistics_data.h>

#include <ui/style/globals.h>
#include <ui/help/help_topics.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/painters/radial_gradient_painter.h>
#include <ui/style/statistics_colors.h>
#include <ui/style/skin.h>
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
        QnStatisticsColors colors = qnGlobals->statisticsColors();
        if (key == QLatin1String("CPU"))
            return colors.cpu;
        if (key == QLatin1String("RAM"))
            return colors.ram;
        return colors.hddByKey(key);
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

        bool first = true;
        bool last = false;
        *currentValue = -1;
        
        QLinkedList<qreal>::const_iterator backPos = values.values.end();
        backPos--;

        for(QLinkedList<qreal>::const_iterator pos = values.values.begin(); pos != values.values.end(); pos++) {
            qreal value = qMin(*pos, 1.0);
            //bool noData = value < 0;
            value = qMax(value, -0.005);
            last = pos == backPos;
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

            // TODO: #GDM This logic seems overly complicated to me.
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

    const int legendImgSize = 24;
    const int legendFontSize = 22;
    const int legendSpacing = 2;
    const int legendMaxSize = 100;
} // anonymous namespace

// -------------------------------------------------------------------------- //
// PtzZoomButtonWidget
// -------------------------------------------------------------------------- //
class LegendButtonWidget: public QnImageButtonWidget {
    typedef QnImageButtonWidget base_type;

public:
    LegendButtonWidget(const QString &key):
        base_type(NULL, 0),
        m_key(key)
    {
        setCheckable(true);
        setProperty(Qn::NoBlockMotionSelection, true);
        setToolTip(key);
        setIcon(qnSkin->icon("item/check.png"));
    }


protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override {
        switch (which) {
        case Qt::MinimumSize:
            return QSizeF(legendImgSize, legendImgSize + legendSpacing);
        case Qt::PreferredSize:
            {
                QFont font;
                font.setPixelSize(legendFontSize);
                return QSizeF(legendImgSize + QFontMetrics(font).width(m_key) + legendSpacing, legendImgSize + legendSpacing);
            }
        case Qt::MaximumSize:
            return QSizeF(legendMaxSize, legendImgSize + legendSpacing);
        default:
            break;
        }
        return base_type::sizeHint(which, constraint);
    }

    qreal stateOpacity(StateFlags stateFlags) {
        return (stateFlags & HOVERED) ? 1.0 : 0.5;
    }

    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) {
        qreal opacity = painter->opacity();
        painter->setOpacity(opacity * (stateOpacity(startState) * (1.0 - progress) + stateOpacity(endState) * progress));

        QRectF imgRect = QRectF(0, 0, legendImgSize, legendImgSize);
        int textOffset = legendImgSize + legendSpacing;
        QRectF textRect = rect.adjusted(textOffset, 0, 0, 0);
        {
            QnScopedPainterPenRollback penRollback(painter, QPen(Qt::black, 2));
            QnScopedPainterBrushRollback brushRollback(painter);

            QColor keyColor = getColorByKey(m_key);
            painter->setBrush(keyColor);
            painter->drawRoundedRect(imgRect, 4, 4);

            QFont font;
            font.setPixelSize(legendFontSize);
            QnScopedPainterFontRollback fontRollback(painter, font);
            painter->setPen(QPen(keyColor, 3));
            painter->drawText(textRect, m_key);
        }
        base_type::paint(painter, startState, endState, progress, widget, imgRect);
        painter->setOpacity(opacity);
    }
private:
    QString m_key;
};

// -------------------------------------------------------------------------- //
// QnServerResourceWidget
// -------------------------------------------------------------------------- //

QnServerResourceWidget::QnServerResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /* = NULL */):
    QnResourceWidget(context, item, parent),
    m_manager(context->instance<QnMediaServerStatisticsManager>()),
    m_lastHistoryId(-1),
    m_counter(0),
    m_renderStatus(Qn::NothingRendered),
    m_maxMaskUsed(1)
{
    m_resource = base_type::resource().dynamicCast<QnMediaServerResource>();
    if(!m_resource) 
        qnCritical("Server resource widget was created with a non-server resource.");

    m_storageLimit = m_manager->storageLimit();
    m_manager->registerServerWidget(m_resource, this, SLOT(at_statistics_received()));

    /* Note that this slot is already connected to nameChanged signal in 
     * base class's constructor.*/
    connect(m_resource.data(), SIGNAL(urlChanged(const QnResourcePtr &)), this, SLOT(updateTitleText()));

    addLegendOverlay();

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

    QRectF legendOffset = painter->combinedTransform().inverted().
            mapRect(QRectF(rect.bottomLeft(), QSizeF(legendImgSize + legendSpacing, legendImgSize + legendSpacing)));
    qreal offset = min / 20.0; //qMax(min / 20.0, legendOffset.height());

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
        grid.setColor(qnGlobals->statisticsColors().grid);
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
            if (!m_checkedFlagByKey.value(key, true))
                continue;

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
        main_pen.setColor(qnGlobals->statisticsColors().frame);
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

        updateLegend();
    }

    m_lastHistoryId = id;
    m_renderStatus = Qn::NewFrameRendered;

    m_elapsedTimer.restart();
    m_counter++;
}

void QnServerResourceWidget::addLegendOverlay() {
    m_legendButtonBar = new QnImageButtonBar(this, 0, Qt::Horizontal);
    connect(m_legendButtonBar, SIGNAL(checkedButtonsChanged()), this, SLOT(at_legend_checkedButtonsChanged()));

    QGraphicsLinearLayout *legendOverlayHLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    legendOverlayHLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    legendOverlayHLayout->addStretch();
    legendOverlayHLayout->addItem(m_legendButtonBar);
    legendOverlayHLayout->addStretch();

    QGraphicsLinearLayout *legendOverlayVLayout = new QGraphicsLinearLayout(Qt::Vertical);
    legendOverlayVLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    legendOverlayVLayout->addStretch();
    legendOverlayVLayout->addItem(legendOverlayHLayout);

    QnViewportBoundWidget *legendOverlayWidget = new QnViewportBoundWidget(this);
    legendOverlayWidget->setLayout(legendOverlayVLayout);
    legendOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
    legendOverlayWidget->setOpacity(1.0);
    addOverlayWidget(legendOverlayWidget, AutoVisible, true);
    legendOverlayWidget->setVisible(true);
}

void QnServerResourceWidget::updateLegend() {

    int visibleMask = 0;
    int checkedMask = 0;

    foreach (QString key, m_sortedKeys) {
        int mask;
        if (!m_buttonMaskByKey.contains(key)) {
            mask = m_maxMaskUsed;
            m_maxMaskUsed *= 2;
            m_buttonMaskByKey[key] = mask;
        } else {
            mask = m_buttonMaskByKey[key];
        }
        visibleMask |= mask;

        if (!m_legendButtonBar->button(mask)) {
            m_legendButtonBar->addButton(mask, new LegendButtonWidget(key));
        }

        bool checked =  m_checkedFlagByKey.value(key, true);
        if (checked)
            checkedMask |= mask;
    }
    m_legendButtonBar->setCheckedButtons(checkedMask);
    m_legendButtonBar->setVisibleButtons(visibleMask);
}

void QnServerResourceWidget::at_legend_checkedButtonsChanged() {
    int checkedMask = m_legendButtonBar->checkedButtons();
    foreach (QString key, m_sortedKeys) {
        if (!m_buttonMaskByKey.contains(key))
            continue;
        int mask = m_buttonMaskByKey[key];
        m_checkedFlagByKey[key] = ((checkedMask & mask) > 0);
    }
}

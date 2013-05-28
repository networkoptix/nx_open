#include "server_resource_widget.h"

#include <utils/math/math.h> /* For M_PI. */
#include <utils/math/color_transformations.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <core/resource/media_server_resource.h>

#include <api/media_server_connection.h>
#include <api/media_server_statistics_manager.h>

#include <ui/animation/variant_animator.h>
#include <ui/animation/opacity_animator.h>
#include <ui/help/help_topics.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/painters/radial_gradient_painter.h>
#include <ui/style/globals.h>
#include <ui/style/statistics_colors.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

namespace {
    /** Convert angle from radians to degrees */
    qreal radiansToDegrees(qreal radian) {
        return (180 * radian) / M_PI;
    }

    /** Get corresponding color from config */
    QColor getDeviceColor(QnStatisticsDeviceType deviceType, const QString &key) {
        QnStatisticsColors colors = qnGlobals->statisticsColors();
        switch (deviceType) {
        case CPU:
            return colors.cpu;
        case RAM:
            return colors.ram;
        case HDD:
            return colors.hddByKey(key);
        case NETWORK_IN:
            return colors.networkInByKey(key);
        case NETWORK_OUT:
            return colors.networkOutByKey(key);
        default:
            return QColor(Qt::white);
        }
    }

    qreal maxValue(const QLinkedList<qreal> &values) {
        qreal result = 0;
        for(QLinkedList<qreal>::const_iterator pos = values.begin(); pos != values.end(); pos++) {
            if (*pos > result)
                result = *pos;
        }
        return result;
    }

    QLinkedList<qreal> scaledNetworkValues(const QLinkedList<qreal> &values, const qreal upperBound) {
        if (qFuzzyIsNull(upperBound))
            return values;

        QLinkedList<qreal> result;
        for(QLinkedList<qreal>::const_iterator pos = values.begin(); pos != values.end(); pos++) {
            if (*pos < 0)
                result << *pos;
            else
                result << *pos/upperBound;
        }
        return result;
    }

    QList<QString> initNetworkSuffixes() {
        QList<QString> result;
        result << QObject::tr("b/s");
        result << QObject::tr("Kb/s");
        result << QObject::tr("Mb/s");
        result << QObject::tr("Gb/s");
        result << QObject::tr("Tb/s");
        return result;
    }
    const QList<QString> networkSuffixes = initNetworkSuffixes();

    QString networkLoadText(const qreal value, qreal upperBound) {
        int idx = 0;
        qreal upper = upperBound / 1000;
        while (upper >= 1.0) {
            upperBound = upper;
            upper = upperBound / 1000;
            idx++;
        }
        idx = qMin(idx, networkSuffixes.size() - 1);
        return QString(QLatin1String("%1 %2")).arg(QString::number(value*upperBound, 'f', 2)).arg(networkSuffixes.at(idx));
    }

    /** Create path for the chart */
    QPainterPath createChartPath(const QLinkedList<qreal> &values, qreal x_step, qreal scale, qreal elapsedStep, qreal *currentValue) {
        QPainterPath path;
        qreal maxValue = -1;
        qreal lastValue = 0;
        const qreal x_step2 = x_step*.5;
        
        qreal x1, y1;
        x1 = -x_step * elapsedStep;

        qreal baseAngle = elapsedStep >= 0.5
            ? radiansToDegrees(qAcos(2 * (1 - elapsedStep))) 
            : radiansToDegrees(qAcos(2 * elapsedStep));

        bool first = true;
        bool last = false;
        *currentValue = -1;
        
        QLinkedList<qreal>::const_iterator backPos = values.end();
        backPos--;

        for(QLinkedList<qreal>::const_iterator pos = values.begin(); pos != values.end(); pos++) {
            qreal value = qMin(*pos, 1.0);
            value = qMax(value, 0.0);
            last = pos == backPos;
            maxValue = qMax(maxValue, value);
            if (first) {
                y1 = value * scale;
                path.moveTo(x1, y1);
                first = false;
                continue;
            }

            /* Drawing only second part of the arc, cut at the beginning */
            bool c1 = x1 + x_step2 < 0.0;

            /* Drawing both parts of the arc, cut at the beginning */
            bool c2 = !c1 && x1 < 0.0;

            /* Drawing both parts of the arc as usual */
            bool c3 = !c1 && !c2 && !last;

            /* Drawing both parts of the arc, cut at the end */
            bool c4 = !c1 && !c2 && !c3 && elapsedStep >= 0.5;

            /* Drawing only first part of the arc, cut at the end */
            bool c5 = !c1 && !c2 && !c3 && !c4;

            qreal y2 = value * scale;
            if (y2 != y1) {
                qreal h = qAbs(y2 - y1);
                qreal angle = (y2 > y1) ? baseAngle : -baseAngle;

                /* Note that we're using 89 degrees for arc length for a reason.
                 * When using full 90 degrees, arcs are connected, but Qt still
                 * inserts an empty line segment to join consecutive arcs.
                 * Path triangulator then chokes when trying to calculate a normal
                 * for this line segment, producing NaNs in its output.
                 * These NaNs are then fed to GPU, resulting in artifacts. */
                qreal a89 = (y2 > y1) ? 89 : -89;
                qreal a90 = (y2 > y1) ? 90 : -90;
                qreal y = qMin(y1, y2);

                if (c1)
                    path.arcMoveTo(x1 + x_step2, y, x_step, h, 180 + angle);
                if (c2)
                    path.arcMoveTo(x1 - x_step2, y, x_step, h, angle);
                if (c2 || c3 || c4)
                    path.arcTo(x1 - x_step2, y, x_step, h, c2 ? angle : a90, c2 ? -angle : -a89);
                if (c1 || c2 || c3 || c4)
                    path.arcTo(x1 + x_step2, y, x_step, h, c1 ? 180 + angle : 180, c1 ? a90 - angle : c4 ? angle : a89);
                if (c5)
                    path.arcTo(x1 - x_step2, y, x_step, h, a90, angle - a90);
            } else {
                if (c1 || c2)
                    path.moveTo(0.0, y1);
                if (c1 || c2 || c3)
                    path.lineTo(x1 + x_step, y2);
                if (c4 || c5)
                    path.lineTo(x1 + x_step * elapsedStep, y2);
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

    /** Backward sorting because buttonBar inserts buttons in reversed order */
    bool statisticsDataLess(const QnStatisticsData &first, const QnStatisticsData &second) {
        if (
                (first.deviceType == NETWORK_IN || first.deviceType == NETWORK_OUT) &&
                (second.deviceType == NETWORK_IN || second.deviceType == NETWORK_OUT)
            )
            return first.description.toLower() > second.description.toLower();

        if (first.deviceType != second.deviceType)
            return first.deviceType > second.deviceType;
        return first.description.toLower() > second.description.toLower();
    }

    class QnBackgroundGradientPainterFactory {
    public:
        QnRadialGradientPainter *operator()(const QGLContext *context) {
            return new QnRadialGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0), context);
        }
    };

    typedef QnGlContextData<QnRadialGradientPainter, QnBackgroundGradientPainterFactory> QnBackgroundGradientPainterStorage;
    Q_GLOBAL_STATIC(QnBackgroundGradientPainterStorage, qn_serverResourceWidget_backgroundGradientPainterStorage)

    const int legendImageSize = 20;
    const int legendFontSize = 20;
    const int itemSpacing = 2;

    const char *buttonBarPropertyName = "_qn_buttonBarPropertyName";

} // anonymous namespace


// -------------------------------------------------------------------------- //
// LegendButtonWidget
// -------------------------------------------------------------------------- //
class LegendButtonWidget: public QnImageButtonWidget {
    typedef QnImageButtonWidget base_type;

public:
    LegendButtonWidget(QnStatisticsDeviceType deviceType, const QString &key):
        base_type(NULL, 0),
        m_deviceType(deviceType),
        m_key(key)
    {
        setCheckable(true);
        setProperty(Qn::NoBlockMotionSelection, true);
        setToolTip(key);
        setIcon(qnSkin->icon("item/check.png"));
    }

    virtual ~LegendButtonWidget() {}

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override {
        switch (which) {
        case Qt::MinimumSize:
            return QSizeF(legendImageSize, legendImageSize + itemSpacing);
        case Qt::PreferredSize: {
            QFont font;
            font.setPixelSize(legendFontSize);
            return QSizeF(legendImageSize + QFontMetrics(font).width(m_key) + itemSpacing*2, legendImageSize + itemSpacing);
        }
        case Qt::MaximumSize: {
            QSizeF hint = base_type::sizeHint(which, constraint);
            return QSizeF(hint.width(), legendImageSize + itemSpacing);
        }
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

        QRectF imgRect = QRectF(0, 0, legendImageSize, legendImageSize);
        int textOffset = legendImageSize + itemSpacing;
        QRectF textRect = rect.adjusted(textOffset, 0, 0, 0);
        {
            //TODO: #GDM Text drawing is very slow. #Elric says it is fast in Qt5
            QnScopedPainterPenRollback penRollback(painter, QPen(Qt::black, 2));
            QnScopedPainterBrushRollback brushRollback(painter);

            QColor keyColor = getDeviceColor(m_deviceType, m_key);
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
    QnStatisticsDeviceType m_deviceType;
    QString m_key;
};


// -------------------------------------------------------------------------- //
// StatisticsOverlayWidget
// -------------------------------------------------------------------------- //
class StatisticsOverlayWidget: public GraphicsWidget {
    typedef GraphicsWidget base_type;
public:
    StatisticsOverlayWidget(QGraphicsItem *parent = NULL, QnServerResourceWidget* widget = NULL):
        base_type(parent),
        m_widget(widget)
    {
    }

    virtual ~StatisticsOverlayWidget() {}

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override {
        return base_type::sizeHint(which == Qt::MinimumSize ? which : Qt::MaximumSize, constraint);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override {
        QnScopedPainterFontRollback fontRollback(painter);
        Q_UNUSED(fontRollback)
        QFont font(this->font());
        font.setPixelSize(legendFontSize);
        painter->setFont(font);

        QRectF rect = option->rect;

        qreal width = rect.width();
        qreal height = rect.height();

        bool isEmpty = m_widget->m_sortedKeys.isEmpty();

        qreal offsetX = isEmpty ? itemSpacing : itemSpacing * 2 + painter->fontMetrics().width(QLatin1String("100%"));
        qreal offsetTop = isEmpty? itemSpacing : painter->fontMetrics().height() + itemSpacing;
        qreal offsetBottom = itemSpacing;

        const qreal grid_pen_width = 2.5;
        const qreal graph_pen_width = 3.0;
        const qreal frame_pen_width = 3.0;

        qreal ow = width - offsetX*2;
        qreal oh = height - offsetTop - offsetBottom;


        if (ow <= 0 || oh <= 0)
            return;

        QRectF inner(offsetX, offsetTop, ow, oh);

        qreal elapsed_step = m_widget->m_renderStatus == Qn::CannotRender ? 0 :
                (qreal)qBound((qreal)0, (qreal)m_widget->m_elapsedTimer.elapsed(), m_widget->m_updatePeriod) / m_widget->m_updatePeriod;

        const qreal x_step = (qreal)ow*1.0/(m_widget->m_pointsLimit - 2); // one point is cut from the beginning and one from the end
        const qreal y_step = oh * 0.025;

        /* Draw grid */
        {
            QPen grid;
            grid.setColor(qnGlobals->statisticsColors().grid);
            grid.setWidthF(grid_pen_width);

            QPainterPath grid_path;
            for (qreal i = offsetX - (x_step * (elapsed_step + m_widget->m_counter%4 - 4)); i < ow + offsetX; i += x_step*4){
                grid_path.moveTo(i, offsetTop);
                grid_path.lineTo(i, oh + offsetTop);
            }
            for (qreal i = y_step*4 + offsetTop; i < oh + offsetTop; i += y_step*4){
                grid_path.moveTo(offsetX, i);
                grid_path.lineTo(ow + offsetX, i);
            }
            painter->strokePath(grid_path, grid);
        }

        QMap<QString, qreal> displayValues;
        qreal maxNetworkValue = 0.0;
        foreach(QString key, m_widget->m_sortedKeys) {
            QnStatisticsData &stats = m_widget->m_history[key];

            LegendButtonBar bar = m_widget->buttonBarByDeviceType(stats.deviceType);
            if (!m_widget->m_checkedFlagByKey[bar].value(key, true))
                continue;

            if (stats.deviceType == NETWORK_IN || stats.deviceType == NETWORK_OUT)
                maxNetworkValue = qMax(maxNetworkValue, maxValue(stats.values));
        }
        qreal networkUpperBound = qFuzzyIsNull(maxNetworkValue) ? 0.0 : 1.0;
        while (maxNetworkValue > networkUpperBound && !qFuzzyIsNull(networkUpperBound))
            networkUpperBound *= 10;

        /* Draw graph lines */
        {
            QnScopedPainterTransformRollback transformRollback(painter);
            Q_UNUSED(transformRollback)

            qreal space_offset = graph_pen_width;

            QTransform graphTransform = painter->transform();
            graphTransform.translate(offsetX, oh + offsetTop - space_offset);
            painter->setTransform(graphTransform);

            QPen graphPen;
            graphPen.setWidthF(graph_pen_width);
            graphPen.setCapStyle(Qt::FlatCap);

            foreach(QString key, m_widget->m_sortedKeys) {
                QnStatisticsData &stats = m_widget->m_history[key];
                LegendButtonBar bar = m_widget->buttonBarByDeviceType(stats.deviceType);

                if (!m_widget->m_checkedFlagByKey[bar].value(key, true))
                    continue;

                QLinkedList<qreal> values = stats.values;
                if (stats.deviceType == NETWORK_IN || stats.deviceType == NETWORK_OUT)
                    values = scaledNetworkValues(values, networkUpperBound);

                qreal currentValue = 0;
                QPainterPath path = createChartPath(values, x_step, -1.0 * (oh - 2*space_offset), elapsed_step, &currentValue);
                displayValues[key] = currentValue;
                graphPen.setColor(getDeviceColor(stats.deviceType, key));
                painter->strokePath(path, graphPen);
            }
        }

        /* Draw frame and numeric values */
        {
            QnScopedPainterPenRollback penRollback(painter);
            Q_UNUSED(penRollback)

            QPen main_pen;
            main_pen.setColor(qnGlobals->statisticsColors().frame);
            main_pen.setWidthF(frame_pen_width);
            main_pen.setJoinStyle(Qt::MiterJoin);

            painter->setPen(main_pen);
            painter->drawRect(inner);

            /* Draw text values on the right side */
            {
                qreal opacity = painter->opacity();
                painter->setOpacity(opacity * m_widget->m_infoOpacity);

                qreal xRight = offsetX + ow + itemSpacing*2;
                qreal xLeft  = itemSpacing;
                foreach(QString key, m_widget->m_sortedKeys) {
                    QnStatisticsData &stats = m_widget->m_history[key];
                    LegendButtonBar bar = m_widget->buttonBarByDeviceType(stats.deviceType);

                    if (!m_widget->m_checkedFlagByKey[bar].value(key, true))
                        continue;

                    qreal interValue = displayValues[key];
                    if (interValue < 0)
                        continue;
                    qreal y = offsetTop + qMax(offsetTop, oh * (1.0 - interValue));


                    main_pen.setColor(getDeviceColor(stats.deviceType, key));
                    painter->setPen(main_pen);

                    if (stats.deviceType == NETWORK_OUT || stats.deviceType == NETWORK_IN) {
                        painter->drawText(xLeft, y, networkLoadText(interValue, networkUpperBound));
                    } else {
                        painter->drawText(xRight, y, tr("%1%").arg(qRound(interValue * 100.0)));
                    }
                }

                if (networkUpperBound > 0) {
                    main_pen.setColor(qnGlobals->statisticsColors().networkLimit);
                    painter->setPen(main_pen);
                    painter->drawText(xLeft, offsetTop*1.5, networkLoadText(1.0, networkUpperBound));
                }

                painter->setOpacity(opacity);
            }
        }
    }

private:
    QnServerResourceWidget* m_widget;
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
    m_infoOpacity(0.0)
{
    for (int i = 0; i < ButtonBarCount; i++)
        m_maxMaskUsed[i] = 1;

    m_resource = base_type::resource().dynamicCast<QnMediaServerResource>();
    if(!m_resource) 
        qnCritical("Server resource widget was created with a non-server resource.");

    m_pointsLimit = m_manager->pointsLimit();
    m_manager->registerServerWidget(m_resource, this, SLOT(at_statistics_received()));
    m_updatePeriod = m_manager->updatePeriod(m_resource);

    /* Note that this slot is already connected to nameChanged signal in 
     * base class's constructor.*/
    connect(m_resource.data(), SIGNAL(urlChanged(const QnResourcePtr &)), this, SLOT(updateTitleText()));

    addOverlays();

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

int QnServerResourceWidget::helpTopicAt(const QPointF &) const {
    return Qn::MainWindow_MonitoringItem_Help;
}

Qn::RenderStatus QnServerResourceWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) {
    Q_UNUSED(channel)
    Q_UNUSED(channelRect)

    drawBackground(paintRect, painter);

    return m_renderStatus;
}

// TODO: #GDM this method draws background only, why 'drawStatistics'?
void QnServerResourceWidget::drawBackground(const QRectF &rect, QPainter *painter) {
    qreal width = rect.width();
    qreal height = rect.height();
    qreal min = qMin(width, height);

    qreal offset = min / 20.0;

    qreal oh = height - offset*2;
    qreal ow = width - offset*2;

    if (ow <= 0 || oh <= 0)
        return;

    QRectF inner(offset, offset, ow, oh);

    /* Draw background */
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
}

void QnServerResourceWidget::addOverlays() {
    StatisticsOverlayWidget* statisticsOverlayWidget = new StatisticsOverlayWidget(this, this);
    statisticsOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);

    QGraphicsLinearLayout *mainOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    mainOverlayLayout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    mainOverlayLayout->setSpacing(0.5);
    mainOverlayLayout->addItem(statisticsOverlayWidget);

    for (int i = 0; i < ButtonBarCount; i++) {
        m_legendButtonBar[i] = new QnImageButtonBar(this, 0, Qt::Horizontal);
        connect(m_legendButtonBar[i], SIGNAL(checkedButtonsChanged()), this, SLOT(at_legend_checkedButtonsChanged()));
        m_legendButtonBar[i]->setOpacity(m_infoOpacity);
        m_legendButtonBar[i]->setProperty(buttonBarPropertyName, i);

        QGraphicsLinearLayout *legendOverlayHLayout = new QGraphicsLinearLayout(Qt::Horizontal);
        legendOverlayHLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
        legendOverlayHLayout->addStretch();
        legendOverlayHLayout->addItem(m_legendButtonBar[i]);
        legendOverlayHLayout->addStretch();
        mainOverlayLayout->addItem(legendOverlayHLayout);
    }

    QnViewportBoundWidget *mainOverlayWidget = new QnViewportBoundWidget(this);
    mainOverlayWidget->setLayout(mainOverlayLayout);
    mainOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
    mainOverlayWidget->setOpacity(1.0);
    addOverlayWidget(mainOverlayWidget, UserVisible, true);
}

LegendButtonBar QnServerResourceWidget::buttonBarByDeviceType(const QnStatisticsDeviceType deviceType) const {
    switch(deviceType) {
    case NETWORK_IN:
        return NetworkInButtonBar;
    case NETWORK_OUT:
        return NetworkOutButtonBar;
    default:
        break;
    }
    return CommonButtonBar;
}

void QnServerResourceWidget::updateLegend() {
    int visibleMask[ButtonBarCount];
    int checkedMask[ButtonBarCount];
    for (int i = 0; i < ButtonBarCount; i++) {
        visibleMask[i] = 0;
        checkedMask[i] = 0;
    }

    foreach (QString key, m_sortedKeys) {
        QnStatisticsData &stats = m_history[key];

        LegendButtonBar bar = buttonBarByDeviceType(stats.deviceType);
        int mask;
        if (!m_buttonMaskByKey[bar].contains(key)) {
            mask = m_maxMaskUsed[bar];
            m_maxMaskUsed[bar] *= 2;
            m_buttonMaskByKey[bar][key] = mask;
        } else {
            mask = m_buttonMaskByKey[bar][key];
        }
        visibleMask[bar] |= mask;

        if (!m_legendButtonBar[bar]->button(mask)) {
            m_legendButtonBar[bar]->addButton(mask, new LegendButtonWidget(stats.deviceType, key));
        }

        bool checked =  m_checkedFlagByKey[bar].value(key, true);
        if (checked)
            checkedMask[bar] |= mask;
    }

    for (int i = 0; i < ButtonBarCount; i++) {
        m_legendButtonBar[i]->setCheckedButtons(checkedMask[i]);
        m_legendButtonBar[i]->setVisibleButtons(visibleMask[i]);
    }

}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QString QnServerResourceWidget::calculateTitleText() const {
    return tr("%1 (%2)").arg(m_resource->getName()).arg(QUrl(m_resource->getUrl()).host());
}

QnResourceWidget::Buttons QnServerResourceWidget::calculateButtonsVisibility() const {
    return base_type::calculateButtonsVisibility() & (CloseButton | RotateButton | InfoButton);
}

QVariant QnServerResourceWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    if(change == QGraphicsItem::ItemSceneHasChanged) {
        connect(opacityAnimator(headerOverlayWidget()), SIGNAL(valueChanged(const QVariant &)), this, SLOT(at_headerOverlayWidget_opacityChanged(const QVariant &)));
    }

    return base_type::itemChange(change, value);
}

void QnServerResourceWidget::at_statistics_received() {
    m_updatePeriod = m_manager->updatePeriod(m_resource);

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

void QnServerResourceWidget::at_legend_checkedButtonsChanged() {
    LegendButtonBar bar = (LegendButtonBar)sender()->property(buttonBarPropertyName).toInt();

    int checkedMask = m_legendButtonBar[bar]->checkedButtons();
    foreach (QString key, m_sortedKeys) {
        if (!m_buttonMaskByKey[bar].contains(key))
            continue;
        int mask = m_buttonMaskByKey[bar][key];
        m_checkedFlagByKey[bar][key] = ((checkedMask & mask) > 0);
    }
}

void QnServerResourceWidget::at_headerOverlayWidget_opacityChanged(const QVariant &value) {
    m_infoOpacity = value.toDouble();
    for (int i = 0; i < ButtonBarCount; i++)
        m_legendButtonBar[i]->setOpacity(m_infoOpacity);
}

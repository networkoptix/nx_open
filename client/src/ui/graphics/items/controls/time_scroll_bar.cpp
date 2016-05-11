#include "time_scroll_bar.h"

#include <QtWidgets/QStyleOptionSlider>

#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/geometry.h>
#include <ui/graphics/items/standard/graphics_scroll_bar_p.h>
#include <ui/style/nx_style.h>

namespace
{
    const qreal indicatorHuntingRadius = 3.0;

} // anonymous namespace

class QnTimeScrollBarPrivate : public GraphicsScrollBarPrivate
{
    Q_DECLARE_PUBLIC(QnTimeScrollBar)
public:
};


QnTimeScrollBar::QnTimeScrollBar(QGraphicsItem *parent):
    base_type(*new QnTimeScrollBarPrivate, Qt::Horizontal, parent),
    m_indicatorPosition(0),
    m_positionMarkerVisible(true)
{
}

QnTimeScrollBar::~QnTimeScrollBar()
{
}

const QnTimeScrollBarColors &QnTimeScrollBar::colors() const
{
    return m_colors;
}

void QnTimeScrollBar::setColors(const QnTimeScrollBarColors &colors)
{
    m_colors = colors;
}

qint64 QnTimeScrollBar::indicatorPosition() const
{
    return m_indicatorPosition;
}

void QnTimeScrollBar::setIndicatorPosition(qint64 indicatorPosition)
{
    m_indicatorPosition = indicatorPosition;
}

void QnTimeScrollBar::paint(QPainter* painter, const QStyleOptionGraphicsItem* _opt, QWidget *widget)
{
    Q_D(QnTimeScrollBar);
    sendPendingMouseMoves(widget);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);

    /* Draw scrollbar background. */
    painter->fillRect(rect(), palette().color(QPalette::Window));

    /* Draw scrollbar handle. */
    QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, NULL);
    handleRect.adjust(0, 1, 0, -1);

    QColor handleColor = m_colors.handle;
    if (auto nxStyle = QnNxStyle::instance())
    {
        QnPaletteColor mainColor = nxStyle->findColor(m_colors.handle);
        if (d->pressedControl)
            handleColor = mainColor.lighter(1);
        else if (opt.state & QStyle::State_MouseOver)
            handleColor = mainColor.lighter(2);
        else
            handleColor = mainColor;
    }

    painter->fillRect(handleRect, handleColor);

    antialiasingRollback.rollback();

    /* Draw indicator. */
    if (m_positionMarkerVisible && m_indicatorPosition >= minimum() && indicatorPosition() <= maximum() + pageStep())
    {
        /* Calculate handle- and groove-relative indicator positions. */
        qint64 handleValue = qBound(0ll, m_indicatorPosition - sliderPosition(), pageStep());
        qint64 grooveValue = m_indicatorPosition - handleValue;

        /* Calculate handle- and groove-relative indicator offsets. */
        qreal grooveOffset = positionFromValue(grooveValue).x();
        qreal handleOffset = GraphicsStyle::sliderPositionFromValue(0, pageStep(), handleValue, handleRect.width(), opt.upsideDown, true);

        /* Paint it. */
        qreal x = handleOffset + grooveOffset;
        painter->setPen(QPen(m_colors.indicator, 2.0));
        painter->drawLine(QPointF(x, opt.rect.top() + 1.0), QPointF(x, opt.rect.bottom())); /* + 1.0 is to deal with AA spilling the line outside the item's boundaries. */
    }
}

void QnTimeScrollBar::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    /* No default context menu. */
}

void QnTimeScrollBar::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mousePressEvent(event);
}

void QnTimeScrollBar::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(QnTimeScrollBar);

    // copy-paste from the base class, but it's needed to prevent double-setting of slider value

    AbstractLinearGraphicsSlider::mouseMoveEvent(event);

    if (!d->pressedControl)
        return;

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    if (!(event->buttons() & Qt::LeftButton
          ||  ((event->buttons() & Qt::MidButton)
               && style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, &opt, NULL))))
        return;

    if (d->pressedControl == QStyle::SC_ScrollBarSlider)
    {
        QRectF sliderRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, NULL);
        qint64 newPosition = valueFromPosition(event->pos() - QnGeometry::cwiseMul(d->relativeClickOffset, sliderRect.size()));
        int m = style()->pixelMetric(QStyle::PM_MaximumDragDistance, &opt, NULL);
        if (m >= 0)
        {
            QRectF r = rect();
            r.adjust(-m, -m, m, m);
            if (!r.contains(event->pos()))
                newPosition = d->snapBackPosition;
        }

        qint64 centerPosition = newPosition + pageStep() / 2;
        qint64 huntingRadius = indicatorHuntingRadius * (maximum() + pageStep() - minimum()) / rect().width();
        if (m_indicatorPosition - huntingRadius < centerPosition && centerPosition < m_indicatorPosition + huntingRadius)
            newPosition = m_indicatorPosition - pageStep() / 2;

        d->setSliderPositionIgnoringAdjustments(newPosition);
    }
    else
    {
        base_type::mouseMoveEvent(event);
    }
}

void QnTimeScrollBar::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mouseReleaseEvent(event);
}

bool QnTimeScrollBar::positionMarkerVisible() const
{
    return m_positionMarkerVisible;
}

void QnTimeScrollBar::setPositionMarkerVisible(bool value)
{
    m_positionMarkerVisible = value;
}

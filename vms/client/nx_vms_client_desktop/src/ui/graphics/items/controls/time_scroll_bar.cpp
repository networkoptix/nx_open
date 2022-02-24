// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_scroll_bar.h"

#include <chrono>

#include <QtWidgets/QStyleOptionSlider>

#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/style/graphics_style.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/palette.h>
#include <qt_graphics_items/graphics_scroll_bar_p.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace nx::vms::client::desktop;
using std::chrono::milliseconds;

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
    m_indicatorVisible(true)
{
    setSkipUpdateOnSliderChange({ SliderRangeChange, SliderStepsChange, SliderValueChange, SliderMappingChange });

    setAcceptHoverEvents(true);

    setPaletteColor(this, QPalette::Dark, colorTheme()->color("dark4"));
    setPaletteColor(this, QPalette::Midlight, colorTheme()->color("dark12"));
    setPaletteColor(this, QPalette::Light, colorTheme()->color("dark16"));
    setPaletteColor(this, QPalette::Text, colorTheme()->color("light4"));
}

QnTimeScrollBar::~QnTimeScrollBar()
{
}

milliseconds QnTimeScrollBar::indicatorPosition() const
{
    return milliseconds(m_indicatorPosition);
}

void QnTimeScrollBar::setIndicatorPosition(milliseconds indicatorPosition)
{
    m_indicatorPosition = indicatorPosition.count();
}

bool QnTimeScrollBar::indicatorVisible() const
{
    return m_indicatorVisible;
}

void QnTimeScrollBar::setIndicatorVisible(bool value)
{
    m_indicatorVisible = value;
}

void QnTimeScrollBar::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    // Draw scrollbar groove and handle.
    base_type::paint(painter, option, widget);

    // Draw handle pixmap.
    QStyleOptionSlider scrollBarOption;
    initStyleOption(&scrollBarOption);
    const auto sliderRect = style()->subControlRect(
        QStyle::CC_ScrollBar,
        &scrollBarOption,
        QStyle::SC_ScrollBarSlider,
        nullptr);

    ensurePixmap(painter->device()->devicePixelRatio());
    const auto size = m_pixmap.size() / m_pixmap.devicePixelRatio();
    const auto rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size, sliderRect);
    painter->drawPixmap(rect.topLeft(), m_pixmap);

    // Draw indicator.
    if (m_indicatorVisible)
    {
        // Calculate handle- and groove-relative indicator positions.
        const auto handleValue = qBound(0ll, m_indicatorPosition - sliderPosition(), pageStep());
        const auto grooveValue = m_indicatorPosition - handleValue;

        // Calculate handle- and groove-relative indicator offsets.
        const auto grooveOffset = positionFromValue(grooveValue).x();
        const auto handleOffset = GraphicsStyle::sliderPositionFromValue(0, pageStep(),
            handleValue, sliderRect.width(), scrollBarOption.upsideDown, true);

        const auto x = handleOffset + grooveOffset;

        // Paint it.
        QnScopedPainterPenRollback penRollback(painter, QPen(palette().text(), 2.0));
        QnScopedPainterAntialiasingRollback aaRollback(painter, false);
        painter->drawLine(x, option->rect.top() + 1.0, x, option->rect.bottom() - 1.0);
    }
}

void QnTimeScrollBar::ensurePixmap(int devicePixelRatio)
{
    static constexpr int kLineWidth = 1;
    static constexpr int kGapWidth = 3;
    static constexpr int kNumLines = 3;

    static constexpr int kHeight = 8;
    static constexpr int kWidth = kLineWidth * kNumLines + kGapWidth * (kNumLines - 1);

    const auto pixmapSize = QSize(kWidth, kHeight) * devicePixelRatio;
    if (m_pixmap.size() == pixmapSize && m_pixmap.devicePixelRatio() == devicePixelRatio)
        return;

    m_pixmap = QPixmap(pixmapSize);
    m_pixmap.setDevicePixelRatio(devicePixelRatio);
    m_pixmap.fill(Qt::transparent);

    QPainter painter(&m_pixmap);
    const auto brush = palette().light();

    QRect stripe(0, 0, kLineWidth, kHeight);
    for (int i = 0; i < kNumLines; ++i)
    {
        painter.fillRect(stripe, brush);
        stripe.moveLeft(stripe.right() + kGapWidth);
    }
}

void QnTimeScrollBar::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    Q_UNUSED(event);
    /* No default context menu. */
}

void QnTimeScrollBar::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    Q_D(QnTimeScrollBar);

    // copy-paste from the base class, but it's needed to prevent double-setting of slider value

    // TODO: #vkutin Figure out if this copypaste is really needed.

    AbstractLinearGraphicsSlider::mouseMoveEvent(event);

    if (!d->pressedControl)
        return;

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    if (!(event->buttons() & Qt::LeftButton
          ||  ((event->buttons() & Qt::MiddleButton)
               && style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, &opt, nullptr))))
        return;

    if (d->pressedControl == QStyle::SC_ScrollBarSlider)
    {
        QRectF sliderRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, nullptr);
        qint64 newPosition = valueFromPosition(event->pos() - nx::vms::client::core::Geometry::cwiseMul(d->relativeClickOffset, sliderRect.size()));
        int m = style()->pixelMetric(QStyle::PM_MaximumDragDistance, &opt, nullptr);
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

void QnTimeScrollBar::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
        m_pixmap = QPixmap();
}

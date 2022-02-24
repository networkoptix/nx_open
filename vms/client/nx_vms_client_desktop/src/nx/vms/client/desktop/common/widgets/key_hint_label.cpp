// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "key_hint_label.h"

#include <QtCore/QEvent>
#include <QtGui/QPainter>

#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

static constexpr int kRoundingRadius = 2;
static constexpr auto kBorderColorName = "dark16";

} // namespace

namespace nx::vms::client::desktop {

KeyHintLabel::KeyHintLabel(QWidget* parent):
    base_type(parent)
{
    setAlignment(Qt::AlignCenter);
}

KeyHintLabel::KeyHintLabel(const QString& text, QWidget* parent):
    base_type(text, parent)
{
    setAlignment(Qt::AlignCenter);
}

QSize KeyHintLabel::minimumSizeHint() const
{
    const auto horizontalBorderMargin = fontMetrics().averageCharWidth();
    const auto verticalBorderMargin = fontMetrics().xHeight() / 2;
    return base_type::minimumSizeHint() + QSize(horizontalBorderMargin, verticalBorderMargin) * 2;
}

void KeyHintLabel::paintEvent(QPaintEvent* event)
{
    base_type::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto lineWidth = fontMetrics().lineWidth();
    const auto lineWidthF = QFontMetricsF(font()).lineWidth();

    QPen borderPen(colorTheme()->color(kBorderColorName));
    borderPen.setWidthF(lineWidthF);
    painter.setPen(borderPen);

    const auto origin = QRect(QPoint(), size() - minimumSizeHint()).center();
    painter.drawRoundedRect(
        QRect(origin, minimumSizeHint()).adjusted(lineWidth, lineWidth, -lineWidth, -lineWidth),
        kRoundingRadius,
        kRoundingRadius);
}

void KeyHintLabel::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);
    if (event->type() == QEvent::FontChange)
        updateGeometry();
}

} // namespace nx::vms::client::desktop

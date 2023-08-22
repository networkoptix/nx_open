// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_recording_indicator.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <QtCore/QDateTime>
#include <QtGui/QPainter>
#include <QtGui/QEnterEvent>
#include <QtGui/QMouseEvent>

namespace {

static const QColor kRecordingBackgroundColor(0xaa, 0x1e, 0x1e);
static const QColor kHoveredBackgroundColor(0x1c, 0x23, 0x27);
static const QColor kPressedBackgroundColor(0x17, 0x1c, 0x1f);

static const QColor kIndicatorColor(0xff, 0xff, 0xff);
static const QColor kStopRectangleColor(0xaa, 0x1e, 0x1e);

constexpr double kInnerCircleRadius = 3;
constexpr double kMinOuterCircleRadius = 5.5;
constexpr double kMaxOuterCircleRadius = 6.5;
constexpr double kOuterCircleWidth = 1.5;

constexpr double kAverageRadius = (kMaxOuterCircleRadius + kMinOuterCircleRadius) / 2;
constexpr double kPulsemplitude = (kMaxOuterCircleRadius - kMinOuterCircleRadius) / 2;

constexpr int kUpdateInterval = 100; //< ms.
constexpr int kPulseLength = 1600; //< ms.

constexpr double kStopRectangleSize = 10;
constexpr double kStopRectangleCournerRadius = 1;

} // namespace

namespace nx::vms::client::desktop {

ScreenRecordingIndicator::ScreenRecordingIndicator(QWidget* parent): base_type(parent)
{
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    timer->start(kUpdateInterval);
}

void ScreenRecordingIndicator::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF rect = this->rect();

    if (underMouse())
    {
        painter.fillRect(rect, isDown() ? kPressedBackgroundColor : kHoveredBackgroundColor);

        painter.setPen({kStopRectangleColor, 0});
        painter.setBrush(kStopRectangleColor);
        painter.drawRoundedRect(
            QRectF{
                QPointF{
                    rect.center().x() - kStopRectangleSize/2,
                    rect.center().y() - kStopRectangleSize/2},
                QSizeF{kStopRectangleSize, kStopRectangleSize}},
            kStopRectangleCournerRadius, kStopRectangleCournerRadius);
    }
    else
    {
        painter.fillRect(rect, kRecordingBackgroundColor);

        const auto pulsePhase = QDateTime::currentMSecsSinceEpoch();
        qreal radius = kAverageRadius + kPulsemplitude * sin(2 * M_PI * pulsePhase / kPulseLength);
        painter.setPen({kIndicatorColor, kOuterCircleWidth});
        painter.drawEllipse(rect.center(), radius, radius);

        painter.setPen({kIndicatorColor, 0});
        painter.setBrush(kIndicatorColor);
        painter.drawEllipse(rect.center(), kInnerCircleRadius, kInnerCircleRadius);
    }
}

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_marker.h"

#include <optional>

#include <QtQuick/QQuickItem>
#include <QtWidgets/QGraphicsView>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/time/formatter.h>

using namespace std::chrono;
using nx::vms::time::Format;

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

static constexpr int kPointerLength = 3;

} // namespace

struct TimeMarker::Private
{
    TimeMarker* const q;

    nx::vms::time::FormatterPtr dateFormatter = nx::vms::time::Formatter::custom(
        QLocale(), nx::vms::time::Formatter::system()->is24HoursTimeFormat());

    std::optional<TimeContent> timeContent;

    int pointerX = 0;
    int bottomY = 0;
    int minX = 0;
    int maxX = 0;

    TimeMarkerDisplay display = TimeMarkerDisplay::compact;

    void applyTimeContent();
    void updatePosition();

    const QmlProperty<QString> dateText{q->widget(), "dateText"};
    const QmlProperty<QString> timeText{q->widget(), "timeText"};
    const QmlProperty<QPointF> pointerPos{q->widget(), "pointerPos"};
    const QmlProperty<qreal> pointerLength{q->widget(), "pointerLength"};
    const QmlProperty<qreal> timestampMs{q->widget(), "timestampMs"};
    const QmlProperty<int> mode{q->widget(), "mode"};
};

TimeMarker::TimeMarker(QnWorkbenchContext* context, QObject* parent):
    TimeMarker(QUrl("Nx/Timeline/private/TimeMarker.qml"), context, parent)
{
    setSuppressedOnMouseClick(false);
}

TimeMarker::TimeMarker(const QUrl& sourceUrl, QnWorkbenchContext* context, QObject* parent):
    base_type(context, sourceUrl, parent),
    d(new Private{this})
{
    setObjectName("TimeSliderTooltip");
    setOrientation(Qt::Vertical);

    if (ini().debugDisableQmlTooltips)
        return;

    widget()->setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

TimeMarker::~TimeMarker()
{
    // Required here for forward declared scoped pointer destruction.
}

bool TimeMarker::isShown() const
{
    return state() == State::shown;
}

void TimeMarker::setShown(bool value)
{
    if (value)
        show();
    else
        hide();
}

QPointF TimeMarker::pointerPos() const
{
    return d->pointerPos();
}

TimeMarker::Mode TimeMarker::mode() const
{
    return (Mode) d->mode.value();
}

void TimeMarker::setMode(Mode value)
{
    if (d->mode == (int) value)
        return;

    d->mode = (int) value;
    d->updatePosition();
}

void TimeMarker::setTimeContent(const TimeContent& value)
{
    d->timeContent = value;
    d->timestampMs = value.archivePosition.count();
    d->applyTimeContent();
}

void TimeMarker::setTimeMarkerDisplay(TimeMarkerDisplay value)
{
    if (d->display == value)
        return;

    d->display = value;
    d->applyTimeContent();
}

void TimeMarker::setPosition(int pointerX, int bottomY, int minX, int maxX)
{
    d->pointerX = pointerX;
    d->bottomY = bottomY;
    d->minX = minX;
    d->maxX = maxX;
    d->updatePosition();
}

void TimeMarker::Private::applyTimeContent()
{
    if (!timeContent)
        return;

    QString dateTextLine;
    QString timeTextLine;

    Format timeFormat;
    if (timeContent->isTimestamp)
    {
        const QDateTime dateTime =
            QDateTime::fromMSecsSinceEpoch(timeContent->displayPosition.count());
        dateTextLine = toString(dateTime.date(), Format::dd_MM_yyyy, dateFormatter);
        timeFormat = timeContent->showMilliseconds ? Format::hh_mm_ss_zzz : Format::hh_mm_ss;
    }
    else
    {
        dateTextLine.clear();
        timeFormat = timeContent->localFileLength >= 1h
            ? (timeContent->showMilliseconds ? Format::hhh_mm_ss_zzz : Format::hhh_mm_ss)
            : (timeContent->showMilliseconds ? Format::mm_ss_zzz : Format::mm_ss);
    }

    timeTextLine = toString(timeContent->displayPosition.count(), timeFormat);

    if (dateTextLine.isEmpty())
    {
        timeText = timeTextLine;
        dateText = {};
        return;
    }

    timeText = timeContent->showDate ? "" : timeTextLine;
    dateText = dateTextLine;
}

void TimeMarker::Private::updatePosition()
{
    switch ((Mode) mode.value())
    {
        case Mode::normal:
        {
            pointerLength = kPointerLength;
            q->setEnclosingRect({minX, 0, maxX - minX, QWIDGETSIZE_MAX});
            break;
        }

        case Mode::leftmost:
        {
            pointerLength = 0;
            q->setEnclosingRect({pointerX, 0, QWIDGETSIZE_MAX, QWIDGETSIZE_MAX});
            break;
        }

        case Mode::rightmost:
        {
            pointerLength = 0;
            q->setEnclosingRect({pointerX - QWIDGETSIZE_MAX, 0, QWIDGETSIZE_MAX, QWIDGETSIZE_MAX});
            break;
        }
    }

    q->setTarget(QPoint(pointerX, bottomY + pointerLength));
}

} // namespace nx::vms::client::desktop::workbench::timeline


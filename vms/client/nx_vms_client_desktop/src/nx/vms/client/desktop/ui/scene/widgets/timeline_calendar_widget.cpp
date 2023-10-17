// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_calendar_widget.h"

#include <nx/vms/client/core/media/time_period_storage.h>
#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

class TimelineCalendarWidget::Private
{
public:
    TimelineCalendarWidget* q = nullptr;

    core::TimePeriodStorage periodStorage;
    core::TimePeriodStorage allCamerasPeriodStorage;
    bool empty = true;

    void updateEmpty();
};

void TimelineCalendarWidget::Private::updateEmpty()
{
    bool result = true;

    for (int type = 0; type < Qn::TimePeriodContentCount; ++type)
    {
        if (!periodStorage.periods((Qn::TimePeriodContent) type).empty()
            || !allCamerasPeriodStorage.periods((Qn::TimePeriodContent) type).empty())
        {
            result = false;
            break;
        }
    }

    if (empty == result)
        return;

    empty = result;
    emit q->emptyChanged();
}

TimelineCalendarWidget::TimelineCalendarWidget(QWidget* parent):
    QQuickWidget(appContext()->qmlEngine(), parent),
    d(new Private{.q = this})
{
    // For semi-transparency:
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);

    setClearColor(Qt::transparent);
    setResizeMode(QQuickWidget::ResizeMode::SizeViewToRootObject);
    setSource(QUrl("Nx/Items/TimelineCalendar.qml"));

    QmlProperty<core::AbstractTimePeriodStorage*>{this, "periodStorage"}.setValue(
        &d->periodStorage);
    QmlProperty<core::AbstractTimePeriodStorage*>{this, "allCamerasPeriodStorage"}.setValue(
        &d->allCamerasPeriodStorage);
    QmlProperty<QLocale>{this, "regionLocale"}.setValue(QLocale::system());
}

TimelineCalendarWidget::~TimelineCalendarWidget()
{
}

void TimelineCalendarWidget::setTimePeriods(
    Qn::TimePeriodContent type, const QnTimePeriodList& periods)
{
    d->periodStorage.setPeriods(type, periods);
    d->updateEmpty();
}

void TimelineCalendarWidget::setAllCamerasTimePeriods(
    Qn::TimePeriodContent type, const QnTimePeriodList& periods)
{
    d->allCamerasPeriodStorage.setPeriods(type, periods);
    d->updateEmpty();
}

bool TimelineCalendarWidget::isEmpty() const
{
    return d->empty;
}

} // namespace nx::vms::client::desktop

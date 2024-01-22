// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuickWidgets/QQuickWidget>

#include <common/common_globals.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/time/date_range.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::desktop {

class TimelineCalendarWidget: public QQuickWidget
{
    Q_OBJECT

public:
    TimelineCalendarWidget(QWidget* parent = nullptr);
    virtual ~TimelineCalendarWidget() override;

    void setTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList& periods);
    void setAllCamerasTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList& periods);

    bool isEmpty() const;

    QmlProperty<core::DateRange> range{this, "range"};
    QmlProperty<core::DateRange> selection{this, "selection"};
    QmlProperty<int> visibleYear{this, "visibleYear"};
    QmlProperty<int> visibleMonth{this, "visibleMonth"};
    QmlProperty<qint64> displayOffset{this, "displayOffset"};
    QmlProperty<QTimeZone> timeZone{this, "timeZone"};
    QmlProperty<bool> periodsVisible{this, "periodsVisible"};
    QmlProperty<qreal> opacity{this, "opacity"};
    QmlProperty<bool> hovered{this, "hovered"};

signals:
    void emptyChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

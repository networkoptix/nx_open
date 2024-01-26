// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class WeekTimeScheduleDialog; }

namespace nx::vms::client::desktop {

class ScheduleCellPainter;

class WeekTimeScheduleDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    WeekTimeScheduleDialog(QWidget* parent, bool isEmptyAllowed = true);
    virtual ~WeekTimeScheduleDialog() override;

    /**
     * @return binary data with schedule. Each hour in a week represented as single bit.
     * Data is converted to HEX string.
     */
    QString scheduleTasks() const;
    void setScheduleTasks(const QString& schedule);

    /**
    * @return binary data with schedule. Each hour in a week represented as single bit.
    */
    QByteArray schedule() const;
    void setSchedule(const QByteArray& schedule);
    void setSchedules(const QVector<QByteArray>& schedules);

private:
    const std::unique_ptr<Ui::WeekTimeScheduleDialog> ui;
    const std::unique_ptr<nx::vms::client::desktop::ScheduleCellPainter> m_scheduleCellPainter;
};

} // namespace nx::vms::client::desktop

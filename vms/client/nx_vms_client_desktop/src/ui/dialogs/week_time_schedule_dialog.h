// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class WeekTimeScheduleDialog; }
namespace nx::vms::client::desktop { class ScheduleCellPainter; }

class QnWeekTimeScheduleDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit QnWeekTimeScheduleDialog(QWidget* parent);
    virtual ~QnWeekTimeScheduleDialog() override;

    /**
     * @return binary data witch schedule. Each hour in a week represented as single bit. Data is
     *     converted to HEX string
     */
    QString scheduleTasks() const;
    void setScheduleTasks(const QString& schedule);

private:
    const std::unique_ptr<Ui::WeekTimeScheduleDialog> ui;
    const std::unique_ptr<nx::vms::client::desktop::ScheduleCellPainter> m_scheduleCellPainter;
};

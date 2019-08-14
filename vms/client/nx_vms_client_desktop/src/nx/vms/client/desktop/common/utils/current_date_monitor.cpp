#include "current_date_monitor.h"

#include <QtCore/QTimer>

namespace {

constexpr std::chrono::milliseconds kCurrentDateCheckInterval{1000};

} // namespace

namespace nx::vms::client::desktop {

CurrentDateMonitor::CurrentDateMonitor(QObject* parent):
    QObject(parent), m_prevDate(QDate::currentDate())
{
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this,
        [this]()
        {
            const auto currentDate = QDate::currentDate();
            if (currentDate == m_prevDate)
                return;
            m_prevDate = currentDate;
            emit currentDateChanged();
        });

    timer->start(kCurrentDateCheckInterval);
}

} // namespace nx::vms::client::desktop

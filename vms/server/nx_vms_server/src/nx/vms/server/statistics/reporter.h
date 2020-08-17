#pragma once

#include <QtCore/QDateTime>

#include <common/common_module_aware.h>
#include <nx_ec/ec_api.h>

#include <nx/utils/timer_manager.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/statistics_data.h>

namespace nx::vms::server::statistics {

class Reporter: public QObject, public QnCommonModuleAware
{
public:
    /** Collects and reports statistics in automatic mode (by internal timer). */
    Reporter(QnCommonModule* serverModule);

    ~Reporter();

    /** Collects statistics from the entire system. */
    ec2::ErrorCode collectReportData(nx::vms::api::SystemStatistics* const outData);

    /** Collects ApiSystemStatistics and sends it to the statistics server. */
    ec2::ErrorCode triggerStatisticsReport(
        const nx::vms::api::StatisticsServerArguments& arguments,
        nx::vms::api::StatisticsServerInfo* const outData);

private:
    void setupTimer();
    void removeTimer();
    void timerEvent();

    QDateTime plannedReportTime(const QDateTime& now);
    ec2::ErrorCode initiateReport(QString* reportApi = nullptr, QnUuid* systemId = nullptr);

private:
    void finishReport(nx::network::http::AsyncHttpClientPtr httpClient);
    nx::vms::api::StatisticsCameraData fullDeviceStatistics(
        nx::vms::api::CameraDataEx&& deviceInfo);

private:
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    bool m_firstTime = true;
    boost::optional<QDateTime> m_plannedReportTime;
    std::chrono::milliseconds m_timerCycle;

    QnMutex m_mutex;
    bool m_timerDisabled = false;
    boost::optional<qint64> m_timerId;
    nx::utils::TimerManager* m_timerManager = nullptr;
};

} // namespace nx::vms::server::statistics

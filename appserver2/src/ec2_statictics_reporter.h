#ifndef EC2_STATICTICS_REPORTER_H
#define EC2_STATICTICS_REPORTER_H

#include <QtCore/QDateTime>

#include <nx/utils/timer_manager.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_statistics.h>


namespace ec2
{
    // TODO: #2.4 remove Ec2 prefix to avoid ec2::Ec2StaticticsReporter
    class Ec2StaticticsReporter
            : public QObject
    {
    public:
        /** Collects and reports statistics in automatic mode (by internal timer) */
        Ec2StaticticsReporter(
            const AbstractResourceManagerPtr& resourceManager,
            const AbstractMediaServerManagerPtr& msManager);

        ~Ec2StaticticsReporter();

        /** Collects \class ApiSystemStatistics in the entire system */
        ErrorCode collectReportData(std::nullptr_t, ApiSystemStatistics* const outData);

        /** Collects \class ApiSystemStatistics and sends it to the statistics server */
        ErrorCode triggerStatisticsReport(std::nullptr_t, ApiStatisticsServerInfo* const outData);

        // server information
        static const QString DEFAULT_SERVER_API;
        static const QString AUTH_USER;
        static const QString AUTH_PASSWORD;

        static QnUuid getDesktopCameraTypeId(const AbstractResourceManagerPtr& manager);

    private:
        void setupTimer();
        void removeTimer();
        void timerEvent();

        QDateTime plannedReportTime(const QDateTime& now);
        ErrorCode initiateReport(QString* reportApi = 0);
        QnUuid getOrCreateSystemId();

    private slots:
        void finishReport(nx_http::AsyncHttpClientPtr httpClient);

    private:
        QnUuid m_desktopCameraTypeId;
        AbstractMediaServerManagerPtr m_msManager;

        nx_http::AsyncHttpClientPtr m_httpClient;
        bool m_firstTime;
        boost::optional<QDateTime> m_plannedReportTime;
        uint m_timerCycle;

        QnMutex m_mutex;
        bool m_timerDisabled;
        boost::optional<qint64> m_timerId;
    };
}

#endif // EC2_STATICTICS_REPORTER_H

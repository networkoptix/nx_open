#ifndef EC2_STATICTICS_REPORTER_H
#define EC2_STATICTICS_REPORTER_H

#include <QtCore/QMutex>

#include <core/resource/resource_fwd.h>
#include <utils/common/timermanager.h>
#include <utils/network/http/asynchttpclient.h>
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
        Ec2StaticticsReporter(const AbstractUserManagerPtr& userManager,
                              const AbstractResourceManagerPtr& resourceManager,
                              const AbstractMediaServerManagerPtr& msManager);
        ~Ec2StaticticsReporter();

        /** Collects \class ApiSystemStatistics in the entire system */
        ErrorCode collectReportData(std::nullptr_t, ApiSystemStatistics* const outData);

        /** Collects \class ApiSystemStatistics and sends it to the statistics server */
        ErrorCode triggerStatisticsReport(std::nullptr_t, ApiStatisticsServerInfo* const outData);

        // text strings
        static const QString SR_LAST_TIME;
        static const QString SR_TIME_CYCLE;
        static const QString SR_SERVER_API;
        static const QString SYSTEM_ID;
        static const QString SYSTEM_NAME_FOR_ID;

        // server information
        static const QString DEFAULT_SERVER_API;
        static const QString AUTH_USER;
        static const QString AUTH_PASSWORD;

        // helpers
        /** Check if user has already made a decision about this system behavior. */
        static bool isDefined(const QnMediaServerResourceList &servers);
        /** Check if statistics reporting is allowed in this system. */
        static bool isAllowed(const QnMediaServerResourceList &servers);
        /** Check if statistics reporting is allowed in this system. */
        static bool isAllowed(const AbstractMediaServerManagerPtr& msManager);
        /** Set allowed value for the following servers. */
        static void setAllowed(const QnMediaServerResourceList &servers, bool value);

        static QnUserResourcePtr getAdmin(const AbstractUserManagerPtr& manager);
        static QnUuid getDesktopCameraTypeId(const AbstractResourceManagerPtr& manager);

    private:
        void setupTimer();
        void removeTimer();
        void timerEvent();

        ErrorCode initiateReport(QString* reportApi = 0);
        QnUuid getOrCreateSystemId();

    private slots:
        void finishReport(nx_http::AsyncHttpClientPtr httpClient);

    private:
        QnUserResourcePtr m_admin;
        QnUuid m_desktopCameraTypeId;
        AbstractMediaServerManagerPtr m_msManager;

        nx_http::AsyncHttpClientPtr m_httpClient;
        boost::optional<QDateTime> m_plannedReportTime;
        uint m_timerCycle;

        QMutex m_mutex;
        bool m_timerDisabled;
        boost::optional<qint64> m_timerId;
    };
}

#endif // EC2_STATICTICS_REPORTER_H

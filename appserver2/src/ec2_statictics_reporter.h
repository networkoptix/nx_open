#ifndef EC2_STATICTICS_REPORTER_H
#define EC2_STATICTICS_REPORTER_H

#include <QtCore/QMutex>

#include "core/resource/user_resource.h"
#include "utils/common/timermanager.h"
#include "utils/network/http/asynchttpclient.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_statistics.h"

namespace ec2
{

    class Ec2StaticticsReporter
            : public QObject
    {
    public:
        Ec2StaticticsReporter(const AbstractUserManagerPtr& userManager,
                                const AbstractResourceManagerPtr& resourceManager);
        ~Ec2StaticticsReporter();

        /** Collects \class ApiSystemStatistics in the entire system */
        ErrorCode collectReportData(std::nullptr_t, ApiSystemStatistics* const outData);

        /** Collects \class ApiSystemStatistics and sends it to the statistics server */
        ErrorCode triggerStatisticsReport(std::nullptr_t, ApiStatisticsServerInfo* const outData);

        // text strings
        static const QString SR_ALLOWED;
        static const QString SR_LAST_TIME;
        static const QString SR_TIME_CYCLE;
        static const QString SR_SERVER_API;
        static const QString SR_SERVER_NO_AUTH;
        static const QString SYSTEM_ID;

    private:
        void setupTimer();
        void removeTimer();
        void timerEvent();

        ErrorCode initiateReport(QString* reportApi = 0);
        QnUuid getOrCreateSystemId();
        uint getTimeSetting(const QString& name, uint defaultValue = 0);

    private slots:
        void finishReport(nx_http::AsyncHttpClientPtr httpClient);

    private:
        QnUserResourcePtr m_admin;
        QnUuid m_desktopCameraTypeId;
        nx_http::AsyncHttpClientPtr m_httpClient;
        boost::optional<uint> m_plannedReportTime;

        QMutex m_mutex;
        bool m_timerDisabled;
        boost::optional<qint64> m_timerId;
    };
}

#endif // EC2_STATICTICS_REPORTER_H

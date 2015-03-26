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

    private:
        void setupTimer(uint delay = 0);
        void timerEvent();
        ErrorCode initiateReport();

    private slots:
        void finishReport(nx_http::AsyncHttpClientPtr httpClient);

    private:
        QnUuid getOrCreateSystemId();

        QnUserResourcePtr m_admin;
        QnUuid m_desktopCameraTypeId;
        nx_http::AsyncHttpClientPtr m_httpClient;

        QMutex m_mutex;
        bool m_timerDisabled;
        boost::optional<qint64> m_timerId;
    };
}

#endif // EC2_STATICTICS_REPORTER_H

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
        Ec2StaticticsReporter(AbstractECConnection& connection);
        ~Ec2StaticticsReporter();

        /** Collects \class ApiSystemStatistics in the entire system */
        ErrorCode collectReportData(std::nullptr_t, ApiSystemStatistics* const outData);

        /** Collects \class ApiSystemStatistics and sends it to the statistics server */
        ErrorCode triggerStatisticsReport(std::nullptr_t, ApiStatisticsServerInfo* const outData);

    private:
        void setUpTimer(uint reportTime);
        ErrorCode initiateReport();

    private slots:
        void finishReport(nx_http::AsyncHttpClientPtr httpClient);

    private:
        QnUserResourcePtr getAdmin();
        QnUuid getResourceTypeIdByName(const QString& name);

        AbstractECConnection& m_connection;
        QnUserResourcePtr m_admin;
        QnUuid m_desktopCamera;

        QMutex m_mutex;
        boost::optional<qint64> m_timerId;
        boost::optional<nx_http::AsyncHttpClientPtr> m_httpClient;
    };
}

#endif // EC2_STATICTICS_REPORTER_H

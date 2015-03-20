#ifndef EC2_STATICTICS_REPORTER_H
#define EC2_STATICTICS_REPORTER_H

#include <QtCore/QMutex>

#include "utils/common/timermanager.h"
#include "utils/network/http/asynchttpclient.h"
#include "nx_ec/ec_api.h"

namespace ec2
{
    // Forward
    class Ec2DirectConnection;

    class Ec2StaticticsReporter
    {
    public:
        Ec2StaticticsReporter(AbstractECConnection& connection);

    private slots:
        void resetTimer();

    private:
        class ReportThread
            : QThread
        {
        public:
            ReportThread(AbstractECConnection& connection);
            virtual void run();

        private slots:
            void sendJsonReport(QUrl url, QJsonValue array);
            void done(nx_http::AsyncHttpClientPtr httpClient);

        private:
            Ec2StaticticsReporter& m_reporter;
        };

        void checkForReportTime(qint64 timerId = 0);

        QnUserResource getAdmin();

        AbstractECConnection& m_connection;
        QMutex m_mutex;

        QnUserResource m_admin;
        boost::optional<qint64> m_timer;
        ReportThread* m_thread;
    };


}

#endif // EC2_STATICTICS_REPORTER_H

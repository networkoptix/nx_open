#include "VideoSessionManager.h"
#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>
#include "QStdIStream.h"
#include "AppSessionManager.h"

VideoServerSessionManager::VideoServerSessionManager(const QHostAddress& host, int port, const QAuthenticator& auth):
    SessionManager(host, port, auth)
{

}

int VideoServerSessionManager::recordedTimePeriods(const QnRequestParamList& params, QnApiRecordedTimePeriodsResponsePtr& timePeriodList)
{
    using xsd::api::recordedTimePeriods::recordedTimePeriods;

    QByteArray reply;

    if(sendGetRequest("api/RecordedTimePeriods", params, reply) == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            timePeriodList = QnApiRecordedTimePeriodsResponsePtr(recordedTimePeriods (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            qDebug(e.what());
        }
    }
    return 1;
}

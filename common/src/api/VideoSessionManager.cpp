#include "VideoSessionManager.h"
#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>
#include "QStdIStream.h"
#include "AppSessionManager.h"

int VideoServerSessionManager::getTimePeriods(const QnRequestParamList& params, QnApiTimePeriodListResponsePtr& timePeriodList)
{
    QByteArray reply;
    /*
    if(sendGetRequest("RecordedTimePeriods", params, reply) == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            QnApiTimePeriodListResponsePtr timePeriodList = QnApiResourceTypeResponsePtr(xsd::api::timePeriodList::TimePeriodList (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            qDebug(e.what());
        }
    }
    */
    return 1;
}

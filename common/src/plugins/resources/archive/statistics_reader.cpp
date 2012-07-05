#include "statistics_reader.h"
#include "utils/common/synctime.h"
#include "core/resource/storage_resource.h"
#include "utils/common/sleep.h"

QnStatisticsReader::QnStatisticsReader(QnResourcePtr resource):
    QnAbstractMediaStreamDataProvider(resource),
    m_api_connection(0)
{
}

QnAbstractMediaDataPtr QnStatisticsReader::getNextData()
{
    if (m_api_connection){
        m_api_connection->asyncGetStatistics(this, SLOT(at_statistics_received(QByteArray)));
    }
    return QnAbstractMediaDataPtr();
}

void QnStatisticsReader::at_statistics_received(const QByteArray &reply){
    qDebug() << "received stats " << reply;
}

void QnStatisticsReader::run()
{
    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        // check queue sizes

        if (!dataCanBeAccepted())
        {
            QnSleep::msleep(5);
            continue;
        }

        QnAbstractMediaDataPtr data = getNextData();
        if(data)
            putData(data);

        QnSleep::msleep(2000);
    }
}

void QnStatisticsReader::setApiConnection(QnVideoServerConnectionPtr connection){
    m_api_connection = connection;
}

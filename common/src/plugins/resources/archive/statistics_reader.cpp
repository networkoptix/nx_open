#include "statistics_reader.h"
#include "utils/common/synctime.h"
#include "core/resource/storage_resource.h"

QnStatisticsReader::QnStatisticsReader(QnResourcePtr resource):
    QnAbstractMediaStreamDataProvider(resource)
{
}

QnAbstractMediaDataPtr QnStatisticsReader::getNextData()
{
    return QnAbstractMediaDataPtr();
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

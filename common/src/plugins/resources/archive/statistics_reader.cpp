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
        m_api_connection->syncGetStatistics(this, SLOT(at_statistics_received(QByteArray)));
    }

    int w(600);
    int h(800);
    QPixmap pixmap(w, h);
 
    QPainter* painter = new QPainter;
    painter->begin(&pixmap);
    drawStatistics(w, h, painter);
    painter->end();    

    QByteArray srcData;
    QBuffer buffer( &srcData );
    buffer.open(  QIODevice::WriteOnly );
    pixmap.save( &buffer, "PNG" );

    QnCompressedVideoDataPtr outData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, srcData.size()));
    outData->data.write(srcData);

    outData->compressionType = CODEC_ID_PNG;
    outData->flags |= AV_PKT_FLAG_KEY | QnAbstractMediaData::MediaFlags_StillImage;
    outData->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    outData->dataProvider = this;
    outData->channelNumber = 0;

    return outData;
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

void QnStatisticsReader::drawStatistics(int width, int height, QPainter *painter){
    QRectF rect(0, 0, width, height);
    painter->setBrush(QBrush(Qt::blue));
    painter->fillRect(rect, Qt::blue);
}
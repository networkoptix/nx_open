#include "statistics_reader.h"
#include "utils/common/synctime.h"
#include "core/resource/storage_resource.h"
#include "utils/common/sleep.h"

#define LIMIT 10
#define STEP 30

QnStatisticsReader::QnStatisticsReader(QnResourcePtr resource):
    QnAbstractMediaStreamDataProvider(resource),
    m_api_connection(0)
{
    for (int i = 0; i < LIMIT; i++)
        m_history.push_back(0);
}

QnAbstractMediaDataPtr QnStatisticsReader::getNextData()
{
    if (m_api_connection){
        m_api_connection->syncGetStatistics(this, SLOT(at_statistics_received(int)));
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

void QnStatisticsReader::at_statistics_received(int usage){
    qDebug() << "received stats " << usage;
    m_history.push_back(usage);
    if (m_history.count() > LIMIT)
        m_history.pop_front();
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
    //painter->setOpacity(.5);

    QRectF rect(0, 0, width, height);
    painter->fillRect(rect, Qt::black);
    
    QPen grid;
    grid.setColor(QColor(0, 255, 0, 127));
    painter->setPen(grid);
    for (int i = STEP/2; i < width; i += STEP){
        painter->drawLine(i, 0, i, height);
    }
    for (int i = STEP/2; i < height; i += STEP){
        painter->drawLine(0, i, width, i);
    }
    
    QPen usage;
    usage.setColor(Qt::green);
    usage.setWidth(2);
    painter->setPen(usage);

    painter->drawRect(rect);

    int x1, y1;
    bool first(true);
    QListIterator<int> iter(m_history);
    while (iter.hasNext()){
        float value = iter.next() / 100.0;
        if (first){
            x1 = 0;
            y1 = height * (1.0 - value);
            first = false;
            continue;
        }

        int x2 = x1 + width/(LIMIT - 1);
        int y2 = height * (1.0 - value);
        painter->drawLine(x1, y1, x2, y2);
        x1 = x2;
        y1 = y2;
    }

}
#include "statistics_reader.h"
#include "utils/common/synctime.h"
#include "core/resource/storage_resource.h"
#include "utils/common/sleep.h"

#define LIMIT 30
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

QPainterPath scalePath(const QPainterPath &source, qreal scaleFactor, qreal offset, qreal height){
    QPainterPath path;
    path.addPath(source);
 //   qreal flip = m_intensity / qreal(100);
    for (int i=0; i<path.elementCount(); ++i)  {
        const QPainterPath::Element &e = path.elementAt(i);
        qreal x = e.x;
        qreal y = offset + height * (1.0 - e.y * scaleFactor);
/*        qreal dx = x - m_pos.x();
        qreal dy = y - m_pos.y();
        qreal len = m_radius - qSqrt(dx * dx + dy * dy);
        if (len > 0)  {
            path.setElementPositionAt(i,
                x + flip * dx * len / m_radius,
                y + flip * dy * len / m_radius);
        } else */ {
            path.setElementPositionAt(i, x, y);
        }
    }
    return path;
}

void QnStatisticsReader::drawStatistics(int width, int height, QPainter *painter){
    //painter->setOpacity(.5);

    bool stretch_y = true;
    bool grid_enabled = true;
    bool background_enabled = true;
    int offset = 20;

    qreal scale_factor = 1.0;

    QRectF rect(0, 0, width, height);
    qreal oh = height - offset*2;
    qreal ow = width - offset*2;

    QRectF inner(offset, offset, ow, oh);

    painter->fillRect(rect, Qt::black);
    painter->setRenderHint(QPainter::Antialiasing);
    
    if (grid_enabled){
        painter->setClipRect(inner);
        QPen grid;
        grid.setColor(QColor(0, 75, 190, 100));
        painter->setPen(grid);
        for (int i = STEP/2; i < width; i += STEP){
            painter->drawLine(i, 0, i, height);
        }
        for (int i = STEP/2; i < height; i += STEP){
            painter->drawLine(0, i, width, i);
        }
    }
    painter->setClipping(false);

    QPen usage;
    usage.setColor(QColor(190, 250, 255));
    usage.setWidth(1);
    painter->setPen(usage);

    painter->drawRect(inner);
 //   usage.setWidth(2);
 //   painter->setPen(usage);

    QPainterPath path;
    path.moveTo(offset, 0.0);
    qreal max_value = 0.01;

    qreal x1, y1;
    bool first(true);
    QListIterator<int> iter(m_history);
    while (iter.hasNext()){
        qreal value = iter.next() / 100.0;
        max_value = qMax(max_value, value);
        if (first){
            x1 = offset;
            y1 = offset + oh * (1.0 - value);
            path.lineTo(x1, value);
            first = false;
            continue;
        }

        qreal x2 = x1 + (qreal)ow*1.0/(LIMIT - 1);
        qreal y2 = offset + oh * (1.0 - value);
        path.lineTo(x2, value);
        x1 = x2;
        y1 = value;
    }
    path.lineTo(offset + ow, y1);
    path.lineTo(offset + ow, 0.0);

    if (stretch_y)
        scale_factor = 1.0 / max_value;

    QPainterPath result_path = scalePath(path, scale_factor, offset, oh);
    
    if (background_enabled){
        QLinearGradient gradient(width / 2, offset + oh, width / 2,  offset + (1.0 - scale_factor)*oh);
         gradient.setColorAt(0.0, QColor(0, 255, 0, 70));
         gradient.setColorAt(0.5, QColor(255, 255, 0, 70));
         gradient.setColorAt(1.0, QColor(255, 0, 0, 70));
         painter->setBrush(gradient);
         painter->drawPath(result_path);
    }
    else
        painter->strokePath(result_path, usage);
}
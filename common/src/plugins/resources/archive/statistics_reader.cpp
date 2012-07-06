#include "statistics_reader.h"
#include "utils/common/synctime.h"
#include "core/resource/storage_resource.h"
#include "utils/common/sleep.h"

#define LIMIT 100
#define STEP 30

QnStatisticsReader::QnStatisticsReader(QnResourcePtr resource):
    QnAbstractMediaStreamDataProvider(resource),
    m_api_connection(0)
{
    for (int i = 0; i < LIMIT; i++)
        m_history.append(-1);
    m_steps.append(1);
    m_steps.append(2);
    m_steps.append(3);
    m_steps.append(4);
    m_steps.append(5);
    m_steps.append(10);
    m_steps.append(15);
    m_steps.append(20);
    m_steps.append(25);
    m_steps.append(30);
    m_steps.append(40);
    m_steps.append(50);
    m_steps.append(60);
    m_steps.append(70);
    m_steps.append(80);
    m_steps.append(90);
    m_steps.append(100);
}

QnAbstractMediaDataPtr QnStatisticsReader::getNextData()
{
    if (m_api_connection){
        m_api_connection->syncGetStatistics(this, SLOT(at_statistics_received(int)));
    }
    int w(1024);
    int h(768);
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
        path.setElementPositionAt(i, x, y);
    }
    return path;
}


QPainterPath shiftPath(const QPainterPath &source, qreal x, qreal y){
    QPainterPath path;
    path.addPath(source);
     for (int i=0; i<path.elementCount(); ++i)  {
        const QPainterPath::Element &e = path.elementAt(i);
        path.setElementPositionAt(i, e.x + x, e.y + y);
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

    qreal radius = qMin(width, height) * 0.5;
    QRadialGradient rgradient(width / 2, height / 2, radius);
    rgradient.setColorAt(0.0, QColor(0, 0, 195, 100));
    rgradient.setColorAt(1, QColor(0, 0, 0, 0));
    painter->setBrush(rgradient);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(width/2, height/2), radius, radius);
    
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


    QPen usage;{
        /*QLinearGradient gradient(0,0, width,  height);
        gradient.setColorAt(0.0, QColor(0, 255, 0));
        gradient.setColorAt(0.5, QColor(255, 255, 0));
        gradient.setColorAt(1.0, QColor(255, 0, 0));*/

        usage.setColor(QColor(190, 250, 255));
        //usage.setBrush(gradient);
        usage.setWidth(2);
        usage.setJoinStyle(Qt::RoundJoin);
        usage.setCapStyle(Qt::RoundCap);
    }
    painter->setPen(usage);

    painter->drawRect(inner);
 //   usage.setWidth(2);
 //   painter->setPen(usage);

    QPainterPath path;
    path.moveTo(offset, 0.0);
    int max_value = stretch_y ? 0 : 100;

    {
        qreal x1, y1;
        const qreal x_step = (qreal)ow*1.0/(LIMIT - 1);
        const qreal x_step2 = x_step / 2;
        qreal value(0.0);
        bool first(true);
        QListIterator<int> iter(m_history);
        while (iter.hasNext()){
            int i_value = iter.next();
            max_value = qMax(max_value, i_value);
            value = i_value / 100.0;
            if (i_value < 0)
                value = 0;
            else if (i_value == 0)
                value = .0001;
            if (first){
                x1 = offset;
                path.lineTo(x1, value);
                first = false;
                y1 = value;
                continue;
            }

            qreal x2 = x1 + x_step;
            path.cubicTo(x1 + x_step2, y1, x1 + x_step2, value, x2, value);
            x1 = x2;
            y1 = value;
        }
        path.lineTo(offset + ow, value);
        path.lineTo(offset + ow, 0.0);
    }

    if (stretch_y){
        QListIterator<int> step(m_steps);
        int value = 1;
        while (step.hasNext()){
            value = step.next();
            if (max_value <= value)
                break;
        }
        max_value = value;
        scale_factor = 100.0 / value;
    }

    QPainterPath result_path = scalePath(path, scale_factor, offset, oh);
 //   QPainterPath shifted_path = shiftPath(result_path, 0, -20);
 //   result_path += shifted_path;

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
    painter->setClipping(false);

    QFont f("times new roman,utopia");
    f.setStyleStrategy(QFont::ForceOutline);
    f.setPointSize(10);
    f.setStyleHint(QFont::Times);
    painter->setFont(f);
    painter->drawText(2, offset, QString::number(max_value)+"%");
}
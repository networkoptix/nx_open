#include "dlink_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "dlink_stream_reader.h"

const char* QnPlDlinkResource::MANUFACTURE = "Dlink";


QnPlDlinkResource::QnPlDlinkResource()
{
    setAuth("admin", "1");
    
    connect(this, SIGNAL(statusChanged(QnResource::Status,QnResource::Status)),
        this, SLOT(onStatusChanged(QnResource::Status,QnResource::Status)));

}

int QnPlDlinkResource::getMaxFps()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_camInfo.possibleFps.size()==0)
    {
        Q_ASSERT(false);
        return 15;
    }

    return m_camInfo.possibleFps.at(0);
}

bool QnPlDlinkResource::isResourceAccessible()
{
    return updateCamInfo();
}

bool QnPlDlinkResource::updateMACAddress()
{
    return true;
}

QString QnPlDlinkResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlDlinkResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnPlDlinkResource::createLiveDataProvider()
{
    //return new MJPEGtreamreader(toSharedPointer(), "ipcam/stream.cgi?nowprofileid=2&audiostream=0");
    //return new MJPEGtreamreader(toSharedPointer(), "video/mjpg.cgi");
    //return new MJPEGtreamreader(toSharedPointer(), "video/mjpg.cgi?profileid=2");
    return new PlDlinkStreamReader(toSharedPointer());
}

void QnPlDlinkResource::setCropingPhysical(QRect croping)
{

}

QnDlink_cam_info QnPlDlinkResource::getCamInfo() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_camInfo;
}

static QStringList getValues(const QString& line)
{
    QStringList result;

    int index = line.indexOf("=");

    if (index < 0)
        return result;

    QString values = line.mid(index+1);

    return values.split(",");
}

static QString getValue(const QString& line)
{
    
    int index = line.indexOf("=");

    if (index < 1)
        return "";

    return line.mid(index+1);
}

static bool sizeCompare(const QSize &s1, const QSize &s2)
{
    return s1.width() > s2.width();
}

bool QnPlDlinkResource::updateCamInfo()
{
    QMutexLocker mutexLocker(&m_mutex);
    QByteArray cam_info_file = downloadFile("config/stream_info.cgi",  getHostAddress(), 80, 1000, getAuth());

    if (cam_info_file.size()==0)
        return false;

    QString file_s(cam_info_file);
    QStringList lines = file_s.split("\r\n", QString::SkipEmptyParts);

    foreach(QString line, lines)
    {
        if (line.contains("videos="))
        {
            if (line.contains("H.264") || line.contains("H264"))
                m_camInfo.hasH264 = true;

            if (line.contains("MPEG4"))
                m_camInfo.hasMPEG4 = true;

        }
        else if (line.contains("resolutions="))
        {
            QStringList vals = getValues(line);
            foreach(const QString& val,  vals)
            {
                QStringList wh_s = val.split("x");
                if (wh_s.size()<2)
                    continue;

                m_camInfo.resolutions.push_back( QSize(wh_s.at(0).toInt(), wh_s.at(1).toInt()) );
            }

        }
        else if (line.contains("framerates="))
        {
            QStringList vals = getValues(line);
            foreach(const QString& val,  vals)
            {
                m_camInfo.possibleFps.push_back( val.toInt() );

            }
        }
        else if (line.contains("vbitrates="))
        {
            QStringList vals = getValues(line);
            foreach(QString bs, vals)
            {
                bool m = bs.toLower().contains("m");
                bool k = bs.toLower().contains("k");

                if (m || k)
                    bs = bs.left(bs.length()-1);
                
                int val = bs.toInt();
                if(m)
                    val *= 1024;

                m_camInfo.possibleBitrates.push_back(val);
            }

            
        }
        else if (line.contains("vprofilenum="))
        {
            m_camInfo.numberOfVideoProfiles = getValue(line).toInt();
        }
        else if (line.contains("qualities="))
        {
            m_camInfo.possibleQualities = getValue(line);

            if (m_camInfo.possibleQualities.toLower().contains("good"))
                m_camInfo.hasFixedQuality = true;
        }
        else if (line.contains("vprofileurl"))
        {
            for(int i = 1; i <= m_camInfo.numberOfVideoProfiles; ++i)
            {
                QString s = QString("vprofileurl") + QString::number(i);
                if (line.contains(s))
                {
                    m_camInfo.videoProfileUrls[i] = getValue(line);
                    break;
                }
            }
        }


    }

    qSort(m_camInfo.possibleBitrates.begin(), m_camInfo.possibleBitrates.end(), qGreater<int>());
    qSort(m_camInfo.possibleFps.begin(), m_camInfo.possibleFps.end(), qGreater<int>());
    qSort(m_camInfo.resolutions.begin(), m_camInfo.resolutions.end(), sizeCompare);

    return true;

}

void QnPlDlinkResource::onStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus)
{
    if (!(oldStatus == Offline && newStatus == Online))
        return;

    QMutexLocker mutexLocker(&m_mutex);
    if (!m_camInfo.inited())
    {
        updateCamInfo();
    }
}
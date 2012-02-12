#include "dlink_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "dlink_stream_reader.h"

const char* QnPlDlinkResource::MANUFACTURE = "Dlink";


QnDlink_cam_info::QnDlink_cam_info():
hasMPEG4(false),
numberOfVideoProfiles(0),
hasFixedQuality(false)
{

}

bool QnDlink_cam_info::inited() const
{
    return numberOfVideoProfiles > 0;
}

// returns resolution with width not less than width
QSize QnDlink_cam_info::resolutionCloseTo(int width)
{
    if (resolutions.size()==0)
    {
        Q_ASSERT(false);
        return QSize(0,0);
    }

    QSize result = resolutions.at(0);


    foreach(const QSize& size, resolutions)
    {
        if (size.width() <= width)
            return result;
        else
            result = size;
    }

    return result;
}

// returns next up bitrate 
QString QnDlink_cam_info::bitrateCloseTo(int val)
{

    QSize result;

    if (possibleBitrates.size()==0)
    {
        Q_ASSERT(false);
        return "";
    }

    QMap<int, QString>::iterator it = possibleBitrates.lowerBound(val);
    if (it == possibleBitrates.end())
        it--;

    return it.value();

}

// returns next up frame rate 
int QnDlink_cam_info::frameRateCloseTo(int fr)
{
    Q_ASSERT(possibleFps.size()>0);

    int result = possibleFps.at(0);

    foreach(int fps, possibleFps)
    {
        if (result <= fr)
            return result;
        else
            result = fps;
    }

    return result;
}


//=======================================================================================

QnPlDlinkResource::QnPlDlinkResource()
{
    setAuth("admin", "1");
}


bool QnPlDlinkResource::isResourceAccessible()
{
    return true;
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

void QnPlDlinkResource::init()
{
    QByteArray cam_info_file = downloadFile("config/stream_info.cgi",  getHostAddress(), 80, 1000, getAuth());
    if (cam_info_file.size()==0)
        return;

    QMutexLocker mutexLocker(&m_mutex);

    QString file_s(cam_info_file);
    QStringList lines = file_s.split("\r\n", QString::SkipEmptyParts);

    foreach(QString line, lines)
    {
        if (line.contains("videos="))
        {
            if (line.contains("H.264"))
                m_camInfo.hasH264 = "H.264";
            else
                if (line.contains("H264"))
                    m_camInfo.hasH264 = "H264";


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

                QString t = bs;

                if (m || k)
                    t = t.left(t.length()-1);
                
                int val = t.toInt();
                if(m)
                    val *= 1024;

                m_camInfo.possibleBitrates[val] = bs;
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


    qSort(m_camInfo.possibleFps.begin(), m_camInfo.possibleFps.end(), qGreater<int>());
    qSort(m_camInfo.resolutions.begin(), m_camInfo.resolutions.end(), sizeCompare);

}

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

void QnDlink_cam_info::clear()
{
    numberOfVideoProfiles = 0;
    hasMPEG4 = false;
    hasH264 = "";
    hasFixedQuality = false;

    videoProfileUrls.clear();
    resolutions.clear();
    possibleBitrates.clear();
    possibleFps.clear();

    possibleQualities = "";
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
            return size;
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
        if (fps <= fr)
        {
            // can return fps or result
            int dif1 = abs(fps-fr);
            int dif2 = abs(result-fr);

            return dif1 <= dif2 ? fps : result;
        }
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
    CLHttpStatus status;
    downloadFile(status, "config/motion.cgi?enable=yes",  getHostAddress(), 80, 1000, getAuth()); // to enable md 

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return;
    }
    
    QByteArray cam_info_file = downloadFile(status, "config/stream_info.cgi",  getHostAddress(), 80, 1000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return;
    }


    if (cam_info_file.size()==0)
        return;

    QMutexLocker mutexLocker(&m_mutex);

    QString file_s(cam_info_file);
    QStringList lines = file_s.split("\r\n", QString::SkipEmptyParts);

    m_camInfo.clear();

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

    //=======remove elements with diff aspect ratio
    if (m_camInfo.resolutions.size() < 2)
        return;


    int w_0 = m_camInfo.resolutions.at(0).width();
    int h_0 = m_camInfo.resolutions.at(0).height();
    float apectRatio_0 = ((float)w_0/h_0);

    int w_1 = m_camInfo.resolutions.at(1).width();
    int h_1 = m_camInfo.resolutions.at(1).height();
    float apectRatio_1 = ((float)w_1/h_1);

    float apectRatio = apectRatio_0;

    if (abs(apectRatio_0 - apectRatio_1) > 0.01)
        apectRatio = apectRatio_1; // 
            
   

    QList<QSize>::iterator it = m_camInfo.resolutions.begin();

    while (it != m_camInfo.resolutions.end()) 
    {
        QSize s = *it;

        float apectRatioL = (float)s.width() / s.height();

        if ( qAbs(apectRatioL - apectRatio) > 0.01 )
            it = m_camInfo.resolutions.erase(it);
        else
            ++it;
    }


}

#include "dlink_resource.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"

const char* QnPlDlinkResource::MANUFACTURE = "Dlink";


QnPlDlinkResource::QnPlDlinkResource()
{
    setAuth("admin", "1");
    
    connect(this, SIGNAL(statusChanged(QnResource::Status,QnResource::Status)),
        this, SLOT(onStatusChanged(QnResource::Status,QnResource::Status)));

}

bool QnPlDlinkResource::isResourceAccessible()
{
    return updateMACAddress();
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
    return new MJPEGtreamreader(toSharedPointer(), "video/mjpg.cgi?profileid=2");
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

void QnPlDlinkResource::updateCamInfo()
{
    QHostAddress addr = getHostAddress();
    QAuthenticator auth = getAuth();


    QMutexLocker mutexLocker(&m_mutex);

    QByteArray cam_info_file = downloadFile("config/stream_info.cgi",  addr, 80, 1000, auth);

    if (cam_info_file.size()==0)
        return;

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
        else if (line.contains("vprofilenum="))
        {
            m_camInfo.numberOfVideoProfiles = getValue(line).toInt();
        }
        else if (line.contains("qualities="))
        {
            m_camInfo.possibleQualities = getValue(line);
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

}

void QnPlDlinkResource::onStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus)
{
    if (!(oldStatus == Offline && newStatus == Online))
        return;

    bool inited ;
    
    {
        QMutexLocker mutexLocker(&m_mutex);
        inited = m_camInfo.inited();
    }
    

    if (inited)
    {
        updateCamInfo();
    }
    

}
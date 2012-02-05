#include "dlink_stream_reader.h"
#include <QTextStream>
#include "dlink_resource.h"

extern int bestBitrateKbps(QnStreamQuality q, QSize resolution, int fps);

PlDlinkStreamReader::PlDlinkStreamReader(QnResourcePtr res):
CLServerPushStreamreader(res),
mHttpClient(0)
{

}

PlDlinkStreamReader::~PlDlinkStreamReader()
{

}


void PlDlinkStreamReader::openStream()
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    
    QString prifileStr = composeVideoProfile();

    QByteArray cam_info_file = downloadFile(prifileStr,  res->getHostAddress(), 80, 1000, res->getAuth());

    
    /*QString fileneme = "config/video.cgi?profileid=";
    if (getRole() == QnResource::Role_SecondaryLiveVideo)
        fileneme += "2";
    else
        fileneme += "1";
        



    bool uploaded = uploadFile(fileneme,  prifileStr, res->getHostAddress(), 80, 1000, res->getAuth());
    if (!uploaded)
        return;

        /**/

}

void PlDlinkStreamReader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
}

bool PlDlinkStreamReader::isStreamOpened() const
{
    return ( mHttpClient && mHttpClient->isOpened() );
}

QnAbstractMediaDataPtr PlDlinkStreamReader::getNextData()
{
    return QnAbstractMediaDataPtr(0);
}

//===============================================
/*
QString PlDlinkStreamReader::composeVideoProfile() const
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (!info.inited())
        return "";

    QnResource::ConnectionRole role = getRole();

    int profileNum;
    QSize resolution;

    if (info.resolutions.size()==0)
    {
        Q_ASSERT(false);
        return "";
    }

    if (info.possibleFps.size()==0)
    {
        Q_ASSERT(false);
        return "";
    }



    if (role == QnResource::Role_SecondaryLiveVideo)
    {
        profileNum = 2;
        resolution = info.resolutionCloseTo(320);
    }
    else
    {
        profileNum = 1;
        resolution = info.resolutions.at(0);
    }


    QString result;
    QTextStream t(&result);

    t << "profileid=" << profileNum << "\r\n";
    //t << "resolution=" << resolution.width() << "x" << resolution.height() << "\r\n";
    t << "resolution=" << 320 << "x" << 240 << "\r\n";
    t << "codec=";

    if (info.hasH264)
    {
        t << "H264\r\n";
    }
    else if (info.hasMPEG4)
    {
        t << "MPEG4\r\n";
    }
    else
    {
        t << "MJPEG\r\n";
    }
    
    int fps = 27;//qMin((int)getFps(), info.possibleFps.at(0));
    t << "framerate=" << fps << "\r\n";

    if (info.hasFixedQuality)
    {
        t << "qualitymode=Fixquality" << "\r\n";

        int q = 60;
        switch (getQuality())
        {
        case QnQualityHighest:
            q = 90;
            break;

        case QnQualityHigh:
            q = 80;
            break;

        case QnQualityNormal:
            q = 70;
            break;

        case QnQualityLow:
            q = 50;
            break;

        case QnQualityLowest:
            q = 40;
            break;
        }

        t << "quality=" << q << "\r\n";
    }
    else
    {
        t << "qualitymode=CBR" << "\r\n";
        t << "bitrate=" << bestBitrateKbps(getQuality(), resolution, fps) << "\r\n";
    }

   

    t.flush();
    return result;

}
/**/


QString PlDlinkStreamReader::composeVideoProfile() const
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (!info.inited())
        return "";

    QnResource::ConnectionRole role = getRole();

    int profileNum;
    QSize resolution;

    if (info.resolutions.size()==0)
    {
        Q_ASSERT(false);
        return "";
    }

    if (info.possibleFps.size()==0)
    {
        Q_ASSERT(false);
        return "";
    }



    if (role == QnResource::Role_SecondaryLiveVideo)
    {
        profileNum = 2;
        resolution = info.resolutionCloseTo(320);
    }
    else
    {
        profileNum = 1;
        resolution = info.resolutions.at(0);
    }


    QString result;
    QTextStream t(&result);

    t << "config/video.cgi?profileid=" << profileNum << "&";
    //t << "resolution=" << resolution.width() << "x" << resolution.height() << "?";
    t << "resolution=" << 800 << "x" << 600 << "&";
    t << "codec=";

    if (info.hasH264)
    {
        t << "H.264&";
    }
    else if (info.hasMPEG4)
    {
        t << "MPEG4&";
    }
    else
    {
        t << "MJPEG&";
    }

    int fps = 7;//qMin((int)getFps(), info.possibleFps.at(0));
    t << "framerate=" << fps << "&";

    if (info.hasFixedQuality && false)
    {
        t << "qualitymode=Fixquality" << "&";

        int q = 60;
        switch (getQuality())
        {
        case QnQualityHighest:
            q = 90;
            break;

        case QnQualityHigh:
            q = 80;
            break;

        case QnQualityNormal:
            q = 70;
            break;

        case QnQualityLow:
            q = 50;
            break;

        case QnQualityLowest:
            q = 40;
            break;
        }

        t << "quality=" << q;
    }
    else
    {
        t << "qualitymode=CBR" << "&";
        t << "bitrate=128K";// << bestBitrateKbps(getQuality(), resolution, fps) << "";
    }



    t.flush();
    return result;

}


void PlDlinkStreamReader::updateStreamParamsBasedOnQuality()
{

}

void PlDlinkStreamReader::updateStreamParamsBasedOnFps()
{

}

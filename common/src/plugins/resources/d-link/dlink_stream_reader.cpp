#include "dlink_stream_reader.h"
#include <QTextStream>
#include "dlink_resource.h"

extern int bestBitrateKbps(QnStreamQuality q, QSize resolution, int fps);

#pragma pack(push,1)
struct ACS_VideoHeader
{
    quint32  ulHdrID; //Header ID
    quint32 ulHdrLength;
    quint32 ulDataLength;
    quint32 ulSequenceNumber;
    quint32 ulTimeSec;
    quint32 ulTimeUSec;
    quint32 ulDataCheckSum;
    quint16 usCodingType;
    quint16 usFrameRate;
    quint16 usWidth;
    quint16 usHeight;
    quint8 ucMDBitmap;
    quint8 ucMDPowers[3];
};

#pragma pack(pop)

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
    if (isStreamOpened())
        return;


    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    
    QString prifileStr = composeVideoProfile();

    QByteArray cam_info_file = downloadFile(prifileStr,  res->getHostAddress(), 80, 1000, res->getAuth());

    if (cam_info_file.length()==0)
        return;

    if (!res->updateCamInfo()) // after we changed profile some videoprofile url might be changed
        return;
    

    // ok, now lets open a stream

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

void PlDlinkStreamReader::updateStreamParamsBasedOnQuality()
{
    bool runing = isRunning();
    pleaseStop();
    stop();
    closeStream();
    
    if (runing)
    {
        start(); // it will call openstream by itself
    }

}

void PlDlinkStreamReader::updateStreamParamsBasedOnFps()
{
    bool runing = isRunning();
    pleaseStop();
    stop();
    closeStream();

    if (runing)
    {
        start(); // it will call openstream by itself
    }

}

//=====================================================================================

QString PlDlinkStreamReader::composeVideoProfile() const
{
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (!info.inited())
        return "";

    QnResource::ConnectionRole role = getRole();

    int profileNum;
    QSize resolution;

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
    t << "resolution=" << resolution.width() << "x" << resolution.height() << "&";

    int fps = info.frameRateCloseTo(getFps());
    t << "framerate=" << fps << "&";
    t << "codec=";


    bool mpeg = false;

    if (info.hasH264!="")
    {
        t << info.hasH264 << "&";
        mpeg = true;
    }
    else if (info.hasMPEG4)
    {
        t << "MPEG4&";
        mpeg = true;
    }
    else
    {
        t << "MJPEG&";
    }



    if (mpeg)
    {
        // just CBR fo mpeg so far
        t << "qualitymode=CBR" << "&";
        t << "bitrate=" <<  info.bitrateCloseTo(bestBitrateKbps(getQuality(), resolution, fps));
    }
    else
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

        t << "quality=" << 1;
    }

    t.flush();
    return result;
}

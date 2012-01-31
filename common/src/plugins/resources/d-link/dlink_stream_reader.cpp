#include "dlink_stream_reader.h"
#include <QTextStream>
#include "dlink_resource.h"

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
    DlinkStreamParams params;
    QnPlDlinkResourcePtr res = getResource().dynamicCast<QnPlDlinkResource>();
    QnDlink_cam_info info = res->getCamInfo();

    if (info.hasH264)
        params.codec = DlinkStreamParams::h264;
    else if (info.hasMPEG4)
        params.codec = DlinkStreamParams::mpeg4;
    else
        params.codec = DlinkStreamParams::mjpeg;

    if (info.possibleFps.size()==0)
    {
        Q_ASSERT(false);
        return;
    }


    if (info.resolutions.size()==0)
    {
        Q_ASSERT(false);
        return;
    }


    params.frameRate = qMin((int)getFps(), info.possibleFps.at(0));



    QnResource::ConnectionRole role = getRole();

    if (role == QnResource::Role_SecondaryLiveVideo)
    {
        params.profileNum = 2;
        params.resolution = info.resolutionCloseTo(320);
    }
    else
    {
        params.profileNum = 1;
        params.resolution = info.resolutions.at(0);
    }

    QString prifileStr = composeVideoProfile(params);


    QString fileneme = "config/video.cgi?profileid=";
    if (params.profileNum==1)
        fileneme += "1";
    else
        fileneme += "2";



    bool uploaded = uploadFile(fileneme,  prifileStr, res->getHostAddress(), 80, 1000, res->getAuth());
    if (!uploaded)
        return;

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
QString PlDlinkStreamReader::composeVideoProfile(const DlinkStreamParams& streamParams) const
{
    QString result;
    QTextStream t(&result);

    t << "profileid=" << streamParams.profileNum << "\r\n";
    t << "resolution=" << streamParams.resolution.width() << "x" << streamParams.resolution.height() << "\r\n";
    t << "codec=";

    switch (streamParams.codec)
    {
    case DlinkStreamParams::h264:
        t << "H264";
    	break;

    case DlinkStreamParams::mpeg4:
        t << "MPEG4";
            break;

    default:
        t << "MJPEG";
    }

    t << "qualitymode=Fixquality" << "\r\n";

    t << "framerate=" << streamParams.frameRate << "\r\n";

    t.flush();
    return result;
}

void PlDlinkStreamReader::updateStreamParamsBasedOnQuality()
{

}

void PlDlinkStreamReader::updateStreamParamsBasedOnFps()
{

}

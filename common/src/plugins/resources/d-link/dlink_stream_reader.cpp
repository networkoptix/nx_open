#include "dlink_stream_reader.h"
#include <QTextStream>
#include "dlink_resource.h"

PlDlinkStreamReader::PlDlinkStreamReader(QnResourcePtr res):
CLServerPushStreamreader(res)
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

    params.frameRate = getFps();
    

    QnResource::ConnectionRole role = getRole();

    if (role == QnResource::Role_SecondaryLiveVideo)
    {

    }




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
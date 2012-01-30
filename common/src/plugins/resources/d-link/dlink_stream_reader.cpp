#include "dlink_stream_reader.h"


PlDlinkStreamReader::PlDlinkStreamReader(QnResourcePtr res):
CLServerPushStreamreader(res)
{

}

PlDlinkStreamReader::~PlDlinkStreamReader()
{

}


void PlDlinkStreamReader::openStream()
{

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

    t << "profileid=" << streamParams.profileNum; << "\r\n";
    t << "resolution=" << streamParams.resolution.width() << "x" << streamParams.resolution.height() << "\r\n";
    t << "codec=";

    switch (streamParams.codec)
    {
    case h264:
        t << "H264";
    	break;

    case mpeg4:
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
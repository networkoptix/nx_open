#include "rtp264_stream_provider.h"

RTP264StreamReader::RTP264StreamReader(QnResourcePtr res, const QString& request):
CLServerPushStreamreader(res),
mRTP264(res),
m_request(request)
{

}

RTP264StreamReader::~RTP264StreamReader()
{

}

void RTP264StreamReader::setRequest(const QString& request)
{
    m_request = request;
}

QnAbstractMediaDataPtr RTP264StreamReader::getNextData() 
{
    return mRTP264.getNextData();
}

void RTP264StreamReader::openStream() 
{
    //mRTP264.setRequest("liveVideoTest");
    //mRTP264.setRequest("stream1");
    mRTP264.setRequest(m_request);
    mRTP264.openStream();
}

void RTP264StreamReader::closeStream() 
{
    mRTP264.closeStream();
}

bool RTP264StreamReader::isStreamOpened() const 
{
    return mRTP264.isStreamOpened();
}

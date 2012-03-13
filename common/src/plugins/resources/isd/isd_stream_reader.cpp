#include "isd_stream_reader.h"


PlISDStreamReader::PlISDStreamReader(QnResourcePtr res):
CLServerPushStreamreader(res),
mRTP264(res)
{

}

PlISDStreamReader::~PlISDStreamReader()
{

}

QnAbstractMediaDataPtr PlISDStreamReader::getNextData() 
{
    return mRTP264.getNextData();
}

void PlISDStreamReader::openStream() 
{
    //mRTP264.setRequest("liveVideoTest");
    mRTP264.setRequest("stream1");
    mRTP264.openStream();
}

void PlISDStreamReader::closeStream() 
{
    mRTP264.closeStream();
}

bool PlISDStreamReader::isStreamOpened() const 
{
    return mRTP264.isStreamOpened();
}

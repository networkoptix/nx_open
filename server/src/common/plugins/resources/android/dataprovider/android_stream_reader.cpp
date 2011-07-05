#include "android_stream_reader.h"
#include "common/base.h"
#include "datapacket/mediadatapacket.h"
#include "network/simple_http_client.h"
#include "resource/network_resource.h"


char jpeg_start[2] = {0xff, 0xd8};
char jpeg_end[2] = {0xff, 0xd9};


int contain_subst(char *data, int datalen, char *subdata, int subdatalen)
{
    if (!data || !subdata || datalen<=0 || subdatalen <= 0 ) 
        return -1;

    int coincidence_len = 0;
    char *pdata = data;


    while(1)
    {
        if (*pdata == subdata[coincidence_len])
            ++coincidence_len;
        else
            coincidence_len = 0;

        if (coincidence_len==subdatalen)
            return (pdata-data - subdatalen + 1);

        if (pdata-data==datalen-1)
            return -1;//not found

        pdata++;

    }
}


CLAndroidStreamreader::CLAndroidStreamreader(QnResource* dev)
:QnServerPushDataProvider(dev),
mHttpClient(0)
{

}

CLAndroidStreamreader::~CLAndroidStreamreader()
{
    
}

QnAbstractMediaDataPacketPtr CLAndroidStreamreader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPacketPtr(0);

    QnCompressedVideoDataPtr videoData( new QnCompressedVideoData(CL_MEDIA_ALIGNMENT,   CL_MAX_DATASIZE/2) );
    CLByteArray& img = videoData->data;


    bool getting_image = false;

    while (isStreamOpened() && !needToStop())
    {
        int readed = 0;

        if (mDataRemainedBeginIndex<0)
            readed = mHttpClient->read(mData, BLOCK_SIZE);
        else
        {
            readed = BLOCK_SIZE - mDataRemainedBeginIndex;
            memmove(mData, mData + mDataRemainedBeginIndex, readed);
            mDataRemainedBeginIndex = -1;
        }

        if (readed < 0 )
            break;


        if (!getting_image)
        {
            int image_index = contain_subst(mData, readed, jpeg_start, 2);
            if (image_index>=0)
            {
                getting_image = true;
                char *to = img.prepareToWrite(readed - image_index); // I assume any image is bigger than BLOCK_SIZE
                memcpy(to, mData + image_index, readed - image_index);
                img.done(readed - image_index);
            }
            else
            {
                // just skip the data
            }
        }
        else // getting the image 
        {

            if (img.size() + BLOCK_SIZE > img.capacity())// too big image
                break;


            int image_end_index = contain_subst(mData, readed, jpeg_end, 2);
            if (image_end_index<0)
            {
                img.write(mData, readed); // I assume any image is bigger than BLOCK_SIZE
            }
            else
            {
                image_end_index+=2;


                // found the end of image 
                getting_image = false;
                img.write(mData, image_end_index);

                if (readed - image_end_index - 1 > 0)
                    mDataRemainedBeginIndex = image_end_index + 1;
                else
                    mDataRemainedBeginIndex = -1;
                    

                videoData->compressionType = CODEC_ID_MJPEG;
                videoData->width = 1920;
                videoData->height = 1088;

                videoData->keyFrame = true;
                videoData->channelNumber = 0;
                videoData->timestamp = QDateTime::currentMSecsSinceEpoch() * 1000;

                return videoData;


            }
        }


    }

    closeStream();
    return QnAbstractMediaDataPacketPtr(0);
}

void CLAndroidStreamreader::openStream()
{
    if (isStreamOpened())
        return;

    QString request = "videofeed";
    CLNetworkDevice* ndev = static_cast<CLNetworkDevice*>(m_device);

    mHttpClient = new CLSimpleHTTPClient(ndev->getIP(), 8080, 2000, ndev->getAuth());
    mHttpClient->setRequestLine(request);
    mHttpClient->openStream();

    mDataRemainedBeginIndex = -1;
}

void CLAndroidStreamreader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
}

bool CLAndroidStreamreader::isStreamOpened() const
{
    return ( mHttpClient && mHttpClient->isOpened() );
}

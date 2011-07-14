#include "iqeye_sp_mjpeg.h"
#include "datapacket/mediadatapacket.h"
#include "common/base.h"
#include "network/simple_http_client.h"
#include "resource/network_resource.h"


extern char jpeg_start[2];
extern char jpeg_end[2];


extern int contain_subst(char *data, int datalen, char *subdata, int subdatalen);



CLIQEyeMJPEGtreamreader::CLIQEyeMJPEGtreamreader(QnResource* dev)
:QnServerPushDataProvider(dev),
mHttpClient(0)
{

}

CLIQEyeMJPEGtreamreader::~CLIQEyeMJPEGtreamreader()
{
    
}

QnAbstractMediaDataPacketPtr CLIQEyeMJPEGtreamreader::getNextData()
{

    if (!isStreamOpened())
        return QnAbstractMediaDataPacketPtr(0);

    QnCompressedVideoDataPtr videoData (new QnCompressedVideoData(CL_MEDIA_ALIGNMENT,   CL_MAX_DATASIZE/2));
    CLByteArray& img = videoData->data;

    bool getting_image = false;



    while (isStreamOpened() && !needToStop())
    {
        if (mDataRemainedBeginIndex<0)
        {
            mReaded = mHttpClient->read(mData, BLOCK_SIZE);
        }
        else
        {
            mReaded = mReaded - mDataRemainedBeginIndex;
            memmove(mData, mData + mDataRemainedBeginIndex, mReaded);
            mDataRemainedBeginIndex = -1;
        }

        
        if (mReaded < 0 )
            break;


        if (!getting_image)
        {

            int image_index = contain_subst(mData, mReaded, jpeg_start, 2);

            if (image_index >=0 )
            {
                getting_image = true;
                char *to = img.prepareToWrite(mReaded - image_index); // I assume any image is bigger than BLOCK_SIZE
                memcpy(to, mData + image_index, mReaded - image_index);
                img.done(mReaded - image_index);
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


            int image_end_index = contain_subst(mData, mReaded, jpeg_end, 2);
            if (image_end_index < 0)
            {
                img.write(mData, mReaded);
            }
            else
            {

                image_end_index+=2;


                // found the end of image 
                getting_image = false;
                img.write(mData, image_end_index);

                if (mReaded - image_end_index > 0)
                    mDataRemainedBeginIndex = image_end_index;
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

void CLIQEyeMJPEGtreamreader::openStream()
{
    if (isStreamOpened())
        return;

    QString request = "now.jpg?snap=spush?dummy=1305868336917";
    QnNetworkResource* ndev = static_cast<QnNetworkResource*>(m_device);

    mHttpClient = new CLSimpleHTTPClient(ndev->getHostAddress(), 80, 2000, ndev->getAuth());
    mHttpClient->setRequestLine(request);
    mHttpClient->openStream();

    mDataRemainedBeginIndex = -1;
}

void CLIQEyeMJPEGtreamreader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
}

bool CLIQEyeMJPEGtreamreader::isStreamOpened() const
{
    return ( mHttpClient && mHttpClient->isOpened() );
}

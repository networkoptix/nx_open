#include "onvif_mjpeg.h"
#include "core/resource/network_resource.h"


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

int contain_subst(char *data, int datalen, int start_index ,  char *subdata, int subdatalen)
{
    int result = contain_subst(data + start_index, datalen - start_index, subdata, subdatalen);

    if (result<0)
        return result;

    return result+start_index;
}



MJPEGtreamreader::MJPEGtreamreader(QnResourcePtr res, const QString& requst)
:CLServerPushStreamreader(res),
mHttpClient(0),
m_request(requst)
{

}

MJPEGtreamreader::~MJPEGtreamreader()
{

}

QnAbstractMediaDataPtr MJPEGtreamreader::getNextData()
{

    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    QnCompressedVideoDataPtr videoData ( new QnCompressedVideoData(CL_MEDIA_ALIGNMENT,   CL_MAX_DATASIZE/2) );
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

                videoData->flags |= AV_PKT_FLAG_KEY;
                videoData->channelNumber = 0;
                videoData->timestamp = QDateTime::currentMSecsSinceEpoch() * 1000;

                return videoData;


            }
        }


    }


    closeStream();

    return QnCompressedVideoDataPtr(0);
}

void MJPEGtreamreader::openStream()
{
    if (isStreamOpened())
        return;

    //QString request = QLatin1String("now.jpg?snap=spush?dummy=1305868336917");
    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();

    mHttpClient = new CLSimpleHTTPClient(nres->getHostAddress(), 80, 2000, nres->getAuth());
    mHttpClient->doGET(m_request);
    //mHttpClient->openStream();

    mDataRemainedBeginIndex = -1;
}

void MJPEGtreamreader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
}

bool MJPEGtreamreader::isStreamOpened() const
{
    return ( mHttpClient && mHttpClient->isOpened() );
}

#include "core/resource/network_resource.h"
#include "utils/common/synctime.h"
#include "utils/common/base.h"
#include "ipwebcam_droid_stream_reader.h"

char jpeg_start[2] = {'\xff', '\xd8'};
char jpeg_end[2] = {'\xff', '\xd9'};

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


QnPlDroidIpWebCamReader::QnPlDroidIpWebCamReader(QnResourcePtr res)
:CLServerPushStreamreader(res),
QnLiveStreamProvider(res),
mHttpClient(0)

{

}

QnPlDroidIpWebCamReader::~QnPlDroidIpWebCamReader()
{
    stop();
}


QnAbstractMediaDataPtr QnPlDroidIpWebCamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, 3*1024*1024+FF_INPUT_BUFFER_PADDING_SIZE));
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

                videoData->flags |= AV_PKT_FLAG_KEY;
                videoData->channelNumber = 0;
                videoData->timestamp = QDateTime::currentMSecsSinceEpoch() * 1000;

                return videoData;


            }
        }


    }


    closeStream();
    return QnAbstractMediaDataPtr(0);

}

void QnPlDroidIpWebCamReader::openStream()
{
    if (isStreamOpened())
        return;

    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();
    mHttpClient = new CLSimpleHTTPClient(nres->getHostAddress(), nres->httpPort() , 2000, nres->getAuth());
    mHttpClient->doGET(QLatin1String("videofeed"));

    mDataRemainedBeginIndex = -1;
}

void QnPlDroidIpWebCamReader::closeStream()
{
    delete mHttpClient;
    mHttpClient = 0;
}

bool QnPlDroidIpWebCamReader::isStreamOpened() const
{
    return ( mHttpClient && mHttpClient->isOpened() );
}

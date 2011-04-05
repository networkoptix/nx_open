#include "avigilon_client_pull.h"
#include "device/network_device.h"
#include "network/simple_http_client.h"
#include "data/mediadata.h"

CLAvigilonStreamreader::CLAvigilonStreamreader(CLDevice* dev):
CLClientPullStreamreader(dev)
{
}

CLAvigilonStreamreader::~CLAvigilonStreamreader()
{

}

CLAbstractMediaData* CLAvigilonStreamreader::getNextData()
{
    //http://192.168.1.99/media/still.jpg

    QString request = "media/still.jpg";
    CLNetworkDevice* ndev = static_cast<CLNetworkDevice*>(m_device);



    CLSimpleHTTPClient http_client(ndev->getIP(), 80, 2000, ndev->getAuth());

    
    http_client.setRequestLine(request);
    http_client.openStream();

    CLCompressedVideoData* videoData = 0;

    if (!http_client.isOpened())
        return 0;

    videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT, qMax(http_client.getContentLen(),  (unsigned int)5*1024*1024));
    CLByteArray& img = videoData->data;

    while(http_client.isOpened())
    {
        int readed = http_client.read(img.prepareToWrite(2000), 2000);

        if (readed<0) // error
        {
            //delete videoData;
            //videoData->releaseRef();
            break;
        }

        img.done(readed);

        if (img.size()>CL_MAX_DATASIZE)
        {
            cl_log.log("Image is too big!!", cl_logERROR);
            //delete videoData;
            videoData->releaseRef();
            return 0;
        }
    }

    img.removeZerrowsAtTheEnd();

    //video/H.264I
    videoData->compressionType = CL_JPEG;
    videoData->width = 1920;
    videoData->height = 1088;

    videoData->keyFrame = true;
    videoData->channelNumber = 0;
    videoData->timestamp = QDateTime::currentMSecsSinceEpoch()*1000;
    return videoData;
}

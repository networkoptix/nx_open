#include "isd_resource.h"
#include "../onvif/dataprovider/rtp_stream_provider.h"


const char* QnPlIsdResource::MANUFACTURE = "ISD";


QnPlIsdResource::QnPlIsdResource()
{
    setAuth("root", "admin");
    
}

bool QnPlIsdResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlIsdResource::updateMACAddress()
{
    return true;
}

QString QnPlIsdResource::manufacture() const
{
    return MANUFACTURE;
}

void QnPlIsdResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

bool QnPlIsdResource::initInternal()
{
    CLHttpStatus status;
    QByteArray reslst1 = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.1.ResolutionList",  getHostAddress(), 80, 1000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }

    QByteArray reslst2 = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.1.ResolutionList",  getHostAddress(), 80, 1000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }

    return true;

}

QnAbstractStreamDataProvider* QnPlIsdResource::createLiveDataProvider()
{

    return new QnRtpStreamReader(toSharedPointer(), "stream1");
}

void QnPlIsdResource::setCropingPhysical(QRect /*croping*/)
{
}

const QnResourceAudioLayout* QnPlIsdResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    if (isAudioEnabled()) {
        const QnRtpStreamReader* rtspReader = dynamic_cast<const QnRtpStreamReader*>(dataProvider);
        if (rtspReader && rtspReader->getDPAudioLayout())
            return rtspReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

#ifdef ENABLE_IQE

#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "iqinvision_resource.h"
#include "../onvif/dataprovider/rtp_stream_provider.h"

const QString QnPlIqResource::MANUFACTURE(lit("IqEye"));


QnPlIqResource::QnPlIqResource()
{
    setVendor(lit("IqEye"));
    setDefaultAuth(lit("root"), lit("system"));
}

QString QnPlIqResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnPlIqResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlIqResource::createLiveDataProvider()
{
    
    //return new MJPEGStreamReader(toSharedPointer(), "mjpg/video.mjpg");

    if (isRtp())
        return new QnRtpStreamReader(toSharedPointer());
        /**/

    return new MJPEGStreamReader(toSharedPointer(), QLatin1String("now.jpg?snap=spush"));
}

void QnPlIqResource::setCroppingPhysical(QRect /*cropping*/)
{

}

CameraDiagnostics::Result QnPlIqResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    const CLHttpStatus status = setOID(QLatin1String("1.2.6.5"), QLatin1String("1")); // Reset crop to maximum size
    switch( status )
    {
        case CL_HTTP_AUTH_REQUIRED:
            return CameraDiagnostics::NotAuthorisedResult( QString() );
        case CL_HTTP_SUCCESS: 
        case CL_HTTP_REDIRECT:
            break;
        default:
            return CameraDiagnostics::UnknownErrorResult();
    }

    //TODO #ak: have to 
        //- find request to receive resolution 
        //- or create data provider and decode first frame to find out params (undesirable)
    /*
    CameraMediaStreams mediaStreams;
    mediaStreams.streams.push_back( CameraMediaStreamInfo(
        PRIMARY_ENCODER_INDEX,
        QSize(1920, 1080),
        isRtp() ? CODEC_ID_H264 : CODEC_ID_MJPEG ) );
    saveMediaStreamInfoIfNeeded( mediaStreams );
    */
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

CLHttpStatus QnPlIqResource::readOID(const QString& oid, QString& result)
{
    QString request = QLatin1String("get.oid?") + oid;

    CLHttpStatus status;
    
    result = QLatin1String(downloadFile(status, request,  getHostAddress(), 80, 1000, getAuth()));

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Qn::Unauthorized);
    }
    
    return status;
}



CLHttpStatus QnPlIqResource::readOID(const QString& oid, int& result)
{
    QString sresult;
    CLHttpStatus status = readOID(oid, sresult);

    result = sresult.toInt();

    return status;
}

CLHttpStatus QnPlIqResource::setOID(const QString& oid, const QString& val)
{
    QString request = QLatin1String("set.oid?OidTR") + oid + QLatin1String("=") + val;
    CLHttpStatus status;

    downloadFile(status, request,  getHostAddress(), 80, 1000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Qn::Unauthorized);
    }

    return status;

}

QSize QnPlIqResource::getMaxResolution() const
{
    QSize s;
    return s;
}

bool QnPlIqResource::isRtp() const
{
    QString name = getModel().toUpper();
    return 
		name == QLatin1String("IQA35") ||
        name == QLatin1String("IQA33N") ||
        //name == QLatin1String("IQA32N") ||
        name == QLatin1String("IQA31") ||
        name == QLatin1String("IQ732N") ||
        name == QLatin1String("IQ732S") ||
        name == QLatin1String("IQ832N") ||
        name == QLatin1String("IQ832S") ||
        name == QLatin1String("IQD30S") ||
        name == QLatin1String("IQD31S") ||
        name == QLatin1String("IQD32S") ||
        name == QLatin1String("IQM30S") ||
        name == QLatin1String("IQM31S") ||
        name == QLatin1String("IQM32N") ||
        //name == QLatin1String("IQ765N") ||
        name == QLatin1String("IQM32S");
}

#endif // #ifdef ENABLE_IQE

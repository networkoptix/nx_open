#ifdef ENABLE_ISD

#include <utils/math/math.h>
#include <nx/network/deprecated/asynchttpclient.h>

#include "isd_stream_reader.h"
#include "isd_resource.h"


const QString QnPlIsdResource::MANUFACTURE(lit("ISD"));
QString QnPlIsdResource::MAX_FPS_PARAM_NAME = QLatin1String("MaxFPS");

static QStringList getValues(const QString& line)
{
    QStringList result;

    int index = line.indexOf(QLatin1Char('='));

    if (index < 0)
        return result;

    QString values = line.mid(index+1);

    return values.split(QLatin1Char(','));
}

static bool sizeCompare(const QSize &s1, const QSize &s2)
{
    return s1.width() > s2.width();
}

// ==================================================================


QnPlIsdResource::QnPlIsdResource()
{
    setVendor(lit("ISD"));
}

void QnPlIsdResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    nx::utils::Url apiUrl;
    apiUrl.setScheme( lit("http") );
    apiUrl.setHost( getHostAddress() );
    apiUrl.setPort( QUrl(getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT) );

    QAuthenticator auth = getAuth();

    apiUrl.setUserName( auth.user());
    apiUrl.setPassword( auth.password() );

    apiUrl.setPath( lit("/api/param.cgi") );
    apiUrl.setQuery( lit("req=Network.1.MacAddress") );

    QString resourceMac = getMAC().toString();
    auto requestCompletionFunc = [resourceMac, completionHandler]
        ( SystemError::ErrorCode osErrorCode, int statusCode, nx::network::http::BufferType msgBody ) mutable
    {
        if( osErrorCode != SystemError::noError ||
            statusCode != nx::network::http::StatusCode::ok )
        {
            return completionHandler( false );
        }

        int sepIndex = msgBody.indexOf('=');
        if( sepIndex == -1 )
            return completionHandler( false );
        QByteArray macAddress = msgBody.mid(sepIndex+1).trimmed();
        completionHandler( macAddress == resourceMac.toLatin1() );
    };

    nx::network::http::downloadFileAsync(
        apiUrl,
        requestCompletionFunc );
}

QString QnPlIsdResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnPlIsdResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

nx::mediaserver::resource::StreamCapabilityMap QnPlIsdResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex /*streamIndex*/)
{
    // TODO: implement me
    return nx::mediaserver::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnPlIsdResource::initializeCameraDriver()
{
    updateDefaultAuthIfEmpty(QLatin1String("root"), QLatin1String("admin"));

    nx::utils::Url apiRequestUrl;
    apiRequestUrl.setScheme( lit("http") );

    QAuthenticator auth = getAuth();

    apiRequestUrl.setUserName( auth.user() );
    apiRequestUrl.setPassword( auth.password() );
    apiRequestUrl.setHost( getHostAddress() );
    apiRequestUrl.setPort( QUrl(getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT) );


    //reading resolution list
    CameraDiagnostics::Result result;

    apiRequestUrl.setPath( lit("/api/param.cgi"));
    apiRequestUrl.setQuery(lit("req=VideoInput.1.h264.1.ResolutionList"));
    QByteArray reslst;
    result = doISDApiRequest( apiRequestUrl, &reslst );
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    QStringList vals = getValues(QLatin1String(reslst));


    QList<QSize> resolutions;

    for(const QString& val:  vals)
    {
        QStringList wh_s = val.split(QLatin1Char('x'));
        if (wh_s.size()<2)
            continue;

        resolutions.push_back( QSize(wh_s.at(0).toInt(), wh_s.at(1).toInt()) );
    }

    if (resolutions.size() < 1 )
        return CameraDiagnostics::CameraInvalidParams(lit("resolution"));



    std::sort(resolutions.begin(), resolutions.end(), sizeCompare);

    {
        QnMutexLocker lock( &m_mutex );
        m_resolution1 = resolutions[0];
        m_resolution2 = QSize(0,0);


        double maxResolutionSquare = m_resolution1.width() * m_resolution1.height();
        double requestSquare = 480 * 316;



        int bestIndex = 0;
        double bestMatchCoeff = maxResolutionSquare / requestSquare;

        float bestAspectRatio = getResolutionAspectRatio(m_resolution1);

        for (int i = 0; i < resolutions.size(); ++i)
        {
            QSize tmp;

            tmp.setWidth( qPower2Ceil(static_cast<unsigned int>(resolutions[i].width()+ 1), 8) );
            tmp.setHeight( qPower2Floor(static_cast<unsigned int>(resolutions[i].height() - 1), 8) );
            float ar1 = getResolutionAspectRatio(tmp);

            tmp.setWidth( qPower2Floor(static_cast<unsigned int>(resolutions[i].width() - 1), 8) );
            tmp.setHeight( qPower2Ceil(static_cast<unsigned int>(resolutions[i].height() + 1), 8) );
            float ar2 = getResolutionAspectRatio(tmp);

            if (!qBetween(qMin(ar1,ar2), bestAspectRatio, qMax(ar1,ar2)))
            {
                continue;
            }

            double square = resolutions[i].width() * resolutions[i].height();


            double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);

            if (matchCoeff <= bestMatchCoeff + 0.002)
            {
                bestIndex = i;
                bestMatchCoeff = matchCoeff;
            }
        }

        if (resolutions[bestIndex].width() * resolutions[bestIndex].height() < 640*480)
            m_resolution2 = resolutions[bestIndex];

    }


    //reading fps list

    apiRequestUrl.setPath(lit("/api/param.cgi"));
    apiRequestUrl.setQuery(lit("req=VideoInput.1.h264.1.FrameRateList"));
    QByteArray fpses;
    result = doISDApiRequest( apiRequestUrl, &fpses );
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    vals = getValues(QLatin1String(fpses));

    QList<int> fpsList;
    for(const QString& fpsS: vals)
    {
        fpsList.push_back(fpsS.trimmed().toInt());
    }

    std::sort(fpsList.begin(),fpsList.end(), std::greater<int>());

    if (fpsList.size()<1)
        return CameraDiagnostics::UnknownErrorResult();

    setMaxFps(fpsList.at(0));

    setProperty(
        Qn::SUPPORTED_MOTION_PARAM_NAME,
        qnResTypePool->getResourceType( getTypeId() )->defaultValue( Qn::SUPPORTED_MOTION_PARAM_NAME ) );


    //reading firmware version

    apiRequestUrl.setPath(lit("/api/param.cgi"));
    apiRequestUrl.setQuery(lit("req=General.FirmwareVersion"));
    QByteArray cameraFirmwareVersion;
    result = doISDApiRequest( apiRequestUrl, &cameraFirmwareVersion );
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    const int sepPos = cameraFirmwareVersion.indexOf('=');
    setFirmware( QLatin1String( sepPos != -1 ? cameraFirmwareVersion.mid( sepPos+1 ) : cameraFirmwareVersion ) );

    setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, 1);
    //setMotionType( Qn::MT_SoftwareGrid );
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

QSize QnPlIsdResource::getPrimaryResolution() const
{
    QnMutexLocker lock( &m_mutex );
    return m_resolution1;
}

QSize QnPlIsdResource::getSecondaryResolution() const
{
    QnMutexLocker lock( &m_mutex );
    return m_resolution2;
}


QnAbstractStreamDataProvider* QnPlIsdResource::createLiveDataProvider()
{
    return new QnISDStreamReader(toSharedPointer());
}

void QnPlIsdResource::setCroppingPhysical(QRect /*cropping*/)
{
}

QnConstResourceAudioLayoutPtr QnPlIsdResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (isAudioEnabled()) {
        const QnISDStreamReader* rtspReader = dynamic_cast<const QnISDStreamReader*>(dataProvider);
        if (rtspReader && rtspReader->getDPAudioLayout())
            return rtspReader->getDPAudioLayout();
        else
            return nx::mediaserver::resource::Camera::getAudioLayout(dataProvider);
    }
    else
        return nx::mediaserver::resource::Camera::getAudioLayout(dataProvider);
}


void QnPlIsdResource::setMaxFps(int f)
{
    setProperty(MAX_FPS_PARAM_NAME, f);
}

CameraDiagnostics::Result QnPlIsdResource::doISDApiRequest( const nx::utils::Url& apiRequestUrl, QByteArray* const msgBody )
{
    int statusCode = nx::network::http::StatusCode::ok;

    SystemError::ErrorCode errorCode = nx::network::http::downloadFileSync(
        apiRequestUrl,
        &statusCode,
        msgBody );
    if( errorCode != SystemError::noError )
        return CameraDiagnostics::ConnectionClosedUnexpectedlyResult( apiRequestUrl.host(), apiRequestUrl.port() );
    if( statusCode == nx::network::http::StatusCode::unauthorized )
    {
        setStatus(Qn::Unauthorized);
        return CameraDiagnostics::NotAuthorisedResult( apiRequestUrl.toString() );
    }
    else if( statusCode != nx::network::http::StatusCode::ok )
    {
        return CameraDiagnostics::CameraResponseParseErrorResult( apiRequestUrl.toString(), apiRequestUrl.path() );
    }

    return CameraDiagnostics::NoErrorResult();
}

#endif // #ifdef ENABLE_ISD

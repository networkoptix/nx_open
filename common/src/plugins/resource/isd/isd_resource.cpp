#ifdef ENABLE_ISD

#include <utils/math/math.h>

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

//==================================================================


QnPlIsdResource::QnPlIsdResource()
{
    setVendor(lit("ISD"));
    setAuth(QLatin1String("root"), QLatin1String("admin"));
}

bool QnPlIsdResource::isResourceAccessible()
{
    return updateMACAddress();
}

QString QnPlIsdResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnPlIsdResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

CameraDiagnostics::Result QnPlIsdResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    CLHttpStatus status;
    int port = QUrl(getUrl()).port(80);
    QByteArray reslst = downloadFile(status, QLatin1String("api/param.cgi?req=VideoInput.1.h264.1.ResolutionList"),  getHostAddress(), port, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Qn::Unauthorized);
        return CameraDiagnostics::UnknownErrorResult();
    }

    QStringList vals = getValues(QLatin1String(reslst));


    QList<QSize> resolutions;

    foreach(const QString& val,  vals)
    {
        QStringList wh_s = val.split(QLatin1Char('x'));
        if (wh_s.size()<2)
            continue;

        resolutions.push_back( QSize(wh_s.at(0).toInt(), wh_s.at(1).toInt()) );
    }

    if (resolutions.size() < 1 )
        return CameraDiagnostics::UnknownErrorResult();



    qSort(resolutions.begin(), resolutions.end(), sizeCompare);

    {
        QMutexLocker lock(&m_mutex);
        m_resolution1 = resolutions[0];
        m_resolution2 = QSize(0,0);


        double maxResolutionSquare = m_resolution1.width() * m_resolution1.height();
        double requestSquare = 480 * 316;



        int bestIndex = -1;
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
    
    
    
       /**/


    /*
    QByteArray reslst2 = downloadFile(status, "api/param.cgi?req=VideoInput.1.h264.2.ResolutionList",  getHostAddress(), port, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Unauthorized);
        return false;
    }
    */


    QByteArray fpses = downloadFile(status, QLatin1String("api/param.cgi?req=VideoInput.1.h264.1.FrameRateList"),  getHostAddress(), port, 3000, getAuth());

    if (status == CL_HTTP_AUTH_REQUIRED)
    {
        setStatus(Qn::Unauthorized);
        QUrl requestedUrl;
        requestedUrl.setHost( getHostAddress() );
        requestedUrl.setPort( port );
        requestedUrl.setScheme( QLatin1String("http") );
        requestedUrl.setPath( QLatin1String("api/param.cgi?req=VideoInput.1.h264.1.FrameRateList") );
        return CameraDiagnostics::NotAuthorisedResult( requestedUrl.toString() );
    }

    /**/
    

    vals = getValues(QLatin1String(fpses));

    QList<int> fpsList;
    foreach(QString fpsS, vals)
    {
        fpsList.push_back(fpsS.trimmed().toInt());
    }

    qSort(fpsList.begin(),fpsList.end(), qGreater<int>());

    if (fpsList.size()<1)
        return CameraDiagnostics::UnknownErrorResult();

    setMaxFps(fpsList.at(0));

    CameraMediaStreams mediaStreams;
    mediaStreams.streams.push_back( CameraMediaStreamInfo( m_resolution1, CODEC_ID_H264 ) );
    if( m_resolution2.width() > 0 )
        mediaStreams.streams.push_back( CameraMediaStreamInfo( m_resolution2, CODEC_ID_H264 ) );
    saveResolutionList( mediaStreams );

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

QSize QnPlIsdResource::getPrimaryResolution() const
{
    QMutexLocker lock(&m_mutex);
    return m_resolution1;
}

QSize QnPlIsdResource::getSecondaryResolution() const
{
    QMutexLocker lock(&m_mutex);
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
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}


void QnPlIsdResource::setMaxFps(int f)
{
    setParam(MAX_FPS_PARAM_NAME, f, QnDomainDatabase);
}

int QnPlIsdResource::getMaxFps() const
{
    QVariant mediaVariant;
    QnSecurityCamResource* this_casted = const_cast<QnPlIsdResource*>(this);

    if (!hasParam(MAX_FPS_PARAM_NAME))
    {
        return QnPhysicalCameraResource::getMaxFps();
    }

    this_casted->getParam(MAX_FPS_PARAM_NAME, mediaVariant, QnDomainMemory);
    return mediaVariant.toInt();
}

#endif // #ifdef ENABLE_ISD

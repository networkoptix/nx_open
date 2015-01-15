#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <utils/common/model_functions.h>
#include <utils/common/util.h>
#include <utils/math/math.h>

#include <api/app_server_connection.h>
#include "nx_ec/dummy_handler.h"
#include "nx_ec/data/api_resource_data.h"
#include "../resource_management/resource_properties.h"
#include "param.h"

static const float MAX_EPS = 0.01f;
static const int MAX_ISSUE_CNT = 3; // max camera issues during a 1 min.
static const qint64 ISSUE_KEEP_TIMEOUT = 1000000ll * 60;

QnVirtualCameraResource::QnVirtualCameraResource():
    m_dtsFactory(0)
{}

#ifdef ENABLE_DATA_PROVIDERS

QnPhysicalCameraResource::QnPhysicalCameraResource(): 
    QnVirtualCameraResource(),
    m_channelNumber(0)
{
    setFlags(Qn::local_live_cam);
}

int QnPhysicalCameraResource::suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const
{
    float lowEnd = 0.1f;
    float hiEnd = 1.0f;

    float qualityFactor = lowEnd + (hiEnd - lowEnd) * (quality - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);

    float resolutionFactor = 0.009f * pow(resolution.width() * resolution.height(), 0.7f);

    float frameRateFactor = fps/1.0f;

    float result = qualityFactor*frameRateFactor * resolutionFactor;

    return qMax(192, result);

    /*
    // I assume for a Qn::QualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a Qn::QualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*10;
    int lowEnd = 1024*1;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;
    frameRateFactor = pow(frameRateFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (quality - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(192,result);
    */
}

int QnPhysicalCameraResource::getChannel() const
{
    QMutexLocker lock(&m_mutex);
    return m_channelNumber;
}

void QnPhysicalCameraResource::setUrl(const QString &urlStr)
{
    QnVirtualCameraResource::setUrl( urlStr ); /* This call emits, so we should not invoke it under lock. */

    QMutexLocker lock(&m_mutex);
    QUrl url( urlStr );
    m_channelNumber = QUrlQuery( url.query() ).queryItemValue( QLatin1String( "channel" ) ).toInt();
    setHttpPort( url.port( httpPort() ) );
    if (m_channelNumber > 0)
        m_channelNumber--; // convert human readable channel in range [1..x] to range [0..x-1]
}

float QnPhysicalCameraResource::getResolutionAspectRatio(const QSize& resolution)
{
    if (resolution.height() == 0)
        return 0;
    float result = static_cast<double>(resolution.width()) / resolution.height();
    // SD NTCS/PAL resolutions have non standart SAR. fix it
    if (resolution.width() == 720 && (resolution.height() == 480 || resolution.height() == 576))
        result = float(4.0 / 3.0);
    return result;
}

QSize QnPhysicalCameraResource::getNearestResolution(const QSize& resolution, float aspectRatio,
                                              double maxResolutionSquare, const QList<QSize>& resolutionList,
                                              double* coeff)
{
	if (coeff)
		*coeff = INT_MAX;

    double requestSquare = resolution.width() * resolution.height();
    if (requestSquare < MAX_EPS || requestSquare > maxResolutionSquare) return EMPTY_RESOLUTION_PAIR;

    int bestIndex = -1;
    double bestMatchCoeff = maxResolutionSquare > MAX_EPS ? (maxResolutionSquare / requestSquare) : INT_MAX;

    for (int i = 0; i < resolutionList.size(); ++i) {
        QSize tmp;

        tmp.setWidth(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].width() + 1), 8));
        tmp.setHeight(qPower2Floor(static_cast<unsigned int>(resolutionList[i].height() - 1), 8));
        float ar1 = getResolutionAspectRatio(tmp);

        tmp.setWidth(qPower2Floor(static_cast<unsigned int>(resolutionList[i].width() - 1), 8));
        tmp.setHeight(qPower2Ceil(static_cast<unsigned int>(resolutionList[i].height() + 1), 8));
        float ar2 = getResolutionAspectRatio(tmp);

        if (aspectRatio != 0 && !qBetween(qMin(ar1,ar2), aspectRatio, qMax(ar1,ar2)))
        {
            continue;
        }

        double square = resolutionList[i].width() * resolutionList[i].height();
        if (square < MAX_EPS) continue;

        double matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff <= bestMatchCoeff + MAX_EPS) {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
            if (coeff)
                *coeff = bestMatchCoeff;
        }
    }

    return bestIndex >= 0 ? resolutionList[bestIndex]: EMPTY_RESOLUTION_PAIR;
}

CameraDiagnostics::Result QnPhysicalCameraResource::initInternal() {
    return CameraDiagnostics::NoErrorResult();
}

void QnPhysicalCameraResource::saveResolutionList( const CameraMediaStreams& supportedNativeStreams )
{
    static const char* RTSP_TRANSPORT_NAME = "rtsp";
    static const char* HLS_TRANSPORT_NAME = "hls";
    static const char* MJPEG_TRANSPORT_NAME = "mjpeg";

    CameraMediaStreams fullStreamList( supportedNativeStreams );
    for( std::vector<CameraMediaStreamInfo>::iterator
        it = fullStreamList.streams.begin();
        it != fullStreamList.streams.end();
         )
    {
        if( it->resolution.isEmpty() || it->resolution == lit("0x0") )
        {
            it = fullStreamList.streams.erase( it );
            continue;
        }

        switch( it->codec )
        {
            case CODEC_ID_H264:
                it->transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
                it->transports.push_back( QLatin1String(HLS_TRANSPORT_NAME) );
                break;
            case CODEC_ID_MPEG4:
                it->transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
                break;
            case CODEC_ID_MJPEG:
                it->transports.push_back( QLatin1String(MJPEG_TRANSPORT_NAME) );
                break;
            default:
                break;
        }

        ++it;
    }

#if !defined(EDGE_SERVER) && !defined(__arm__)
#define TRANSCODING_AVAILABLE
#endif

#ifdef TRANSCODING_AVAILABLE
    static const char* WEBM_TRANSPORT_NAME = "webm";

    CameraMediaStreamInfo transcodedStream;
    //any resolution is supported
    transcodedStream.transports.push_back( QLatin1String(MJPEG_TRANSPORT_NAME) );
    transcodedStream.transports.push_back( QLatin1String(WEBM_TRANSPORT_NAME) );
    transcodedStream.transcodingRequired = true;
    fullStreamList.streams.push_back( transcodedStream );
#endif

    //saving fullStreamList;
    QByteArray serializedStreams = QJson::serialized( fullStreamList );
    setProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME, QString::fromUtf8(serializedStreams));
}


CameraMediaStreamInfo::CameraMediaStreamInfo()
:
    resolution( lit("*") ),
    transcodingRequired( false ),
    codec( CODEC_ID_NONE )
{
}

CameraMediaStreamInfo::CameraMediaStreamInfo( const QSize& _resolution, CodecID _codec )
:
    resolution( _resolution.isValid() ? QString::fromLatin1("%1x%2").arg(_resolution.width()).arg(_resolution.height()) : lit("*") ),
    transcodingRequired( false ),
    codec( _codec )
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( (CameraMediaStreamInfo)(CameraMediaStreams), (json), _Fields )


#endif // ENABLE_DATA_PROVIDERS

// --------------- QnVirtualCameraResource ----------------------

QnAbstractDTSFactory* QnVirtualCameraResource::getDTSFactory()
{
    return m_dtsFactory;
}

void QnVirtualCameraResource::setDTSFactory(QnAbstractDTSFactory* factory)
{
    m_dtsFactory = factory;
}

void QnVirtualCameraResource::lockDTSFactory()
{
    m_mutex.lock();
}

void QnVirtualCameraResource::unLockDTSFactory()
{
    m_mutex.unlock();
}


QString QnVirtualCameraResource::getUniqueId() const
{
    return getPhysicalId();
}

bool QnVirtualCameraResource::isForcedAudioSupported() const {
    QString val = getProperty(Qn::FORCED_IS_AUDIO_SUPPORTED_PARAM_NAME);
    return val.toUInt() > 0;
}

void QnVirtualCameraResource::forceEnableAudio()
{ 
	if (isForcedAudioSupported())
        return;
    setProperty(Qn::FORCED_IS_AUDIO_SUPPORTED_PARAM_NAME, 1);
    saveParams(); 
}

void QnVirtualCameraResource::forceDisableAudio()
{ 
    if (!isForcedAudioSupported())
        return;
    setProperty(Qn::FORCED_IS_AUDIO_SUPPORTED_PARAM_NAME, QString(lit("0")));
    saveParams(); 
}

void QnVirtualCameraResource::saveParams()
{
    propertyDictionary->saveParams(getId());
}

void QnVirtualCameraResource::saveParamsAsync()
{
    propertyDictionary->saveParamsAsync(getId());
}

QString QnVirtualCameraResource::toSearchString() const
{
    QString result;
    QTextStream(&result) << QnNetworkResource::toSearchString() << " " << getModel() << " " << getFirmware() << " " << getVendor(); //TODO: #Elric evil!
    return result;
}


void QnVirtualCameraResource::issueOccured()
{
    QMutexLocker lock(&m_mutex);
    m_issueTimes.push_back(getUsecTimer());
    if (m_issueTimes.size() >= MAX_ISSUE_CNT) {
        if (!hasStatusFlags(Qn::CSF_HasIssuesFlag)) {
            addStatusFlags(Qn::CSF_HasIssuesFlag);
            lock.unlock();
            saveAsync();
        }
    }
}

int QnVirtualCameraResource::saveAsync()
{
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    return conn->getCameraManager()->addCamera(::toSharedPointer(this), this, &QnVirtualCameraResource::at_saveAsyncFinished);
}

void QnVirtualCameraResource::at_saveAsyncFinished(int, ec2::ErrorCode, const QnVirtualCameraResourceList &)
{
    // not used
}

void QnVirtualCameraResource::noCameraIssues()
{
    QMutexLocker lock(&m_mutex);
    qint64 threshold = getUsecTimer() - ISSUE_KEEP_TIMEOUT;
    while(!m_issueTimes.empty() && m_issueTimes.front() < threshold)
        m_issueTimes.pop_front();

    if (m_issueTimes.empty() && hasStatusFlags(Qn::CSF_HasIssuesFlag)) {
        removeStatusFlags(Qn::CSF_HasIssuesFlag);
        lock.unlock();
        saveAsync();
    }
}



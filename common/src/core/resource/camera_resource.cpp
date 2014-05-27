#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <utils/common/model_functions.h>
#include <utils/math/math.h>
#include <api/app_server_connection.h>


static const float MAX_EPS = 0.01f;
static const int MAX_ISSUE_CNT = 3; // max camera issues during a 1 min.
static const qint64 ISSUE_KEEP_TIMEOUT = 1000000ll * 60;

QnVirtualCameraResource::QnVirtualCameraResource():
    m_dtsFactory(0)
{}

QnPhysicalCameraResource::QnPhysicalCameraResource(): 
    QnVirtualCameraResource(),
    m_channelNumber(0)
{
    setFlags(local_live_cam);
}

int QnPhysicalCameraResource::suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const
{
    // I assume for a Qn::QualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a Qn::QualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*10;
    int lowEnd = 1024*1;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;
    frameRateFactor = pow(frameRateFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (q - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(192,result);
}

int QnPhysicalCameraResource::getChannel() const
{
    QMutexLocker lock(&m_mutex);
    return m_channelNumber;
}

void QnPhysicalCameraResource::setUrl(const QString &url)
{
    QnVirtualCameraResource::setUrl(url); /* This call emits, so we should not invoke it under lock. */

    QMutexLocker lock(&m_mutex);
    m_channelNumber = QUrlQuery(QUrl(url).query()).queryItemValue(QLatin1String("channel")).toInt();
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
                                              double maxResolutionSquare, const QList<QSize>& resolutionList)
{
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
    static const char* WEBM_TRANSPORT_NAME = "webm";

    static const char* CAMERA_MEDIA_STREAM_LIST_PARAM_NAME = "mediaStreams";

    CameraMediaStreams fullStreamList( supportedNativeStreams );
    for( CameraMediaStreamInfo& streamInfo: fullStreamList.streams )
    {
        switch( streamInfo.codec )
        {
            case CODEC_ID_H264:
                streamInfo.transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
                streamInfo.transports.push_back( QLatin1String(HLS_TRANSPORT_NAME) );
                break;
            case CODEC_ID_MPEG4:
                streamInfo.transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
                break;
            case CODEC_ID_MJPEG:
                streamInfo.transports.push_back( QLatin1String(MJPEG_TRANSPORT_NAME) );
                break;
            default:
                break;
        }
    }

#if !defined(EDGE_SERVER) && !defined(__arm__)
#define TRANSCODING_AVAILABLE
#endif

#ifdef TRANSCODING_AVAILABLE
    CameraMediaStreamInfo transcodedStream;
    //any resolution is supported
    transcodedStream.transports.push_back( QLatin1String(MJPEG_TRANSPORT_NAME) );
    transcodedStream.transports.push_back( QLatin1String(WEBM_TRANSPORT_NAME) );
    transcodedStream.transcodingRequired = true;
    fullStreamList.streams.push_back( transcodedStream );
#endif

    //saving fullStreamList;
    const QByteArray& serializedStreams = QJson::serialized( fullStreamList );
    setParam( QLatin1String(CAMERA_MEDIA_STREAM_LIST_PARAM_NAME), QLatin1String(serializedStreams), QnDomainDatabase );
}

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
    QVariant val;
    if (!getParam(lit("forcedIsAudioSupported"), val, QnDomainMemory))
        return false;
    return val.toUInt() > 0;
}

void QnVirtualCameraResource::forceEnableAudio()
{ 
	if (isForcedAudioSupported())
        return;
    setParam(lit("forcedIsAudioSupported"), 1, QnDomainDatabase); 
    saveParams(); 
}

void QnVirtualCameraResource::forceDisableAudio()
{ 
    if (!isForcedAudioSupported())
        return;
    setParam(lit("forcedIsAudioSupported"), 0, QnDomainDatabase); 
    saveParams(); 
}

void QnVirtualCameraResource::saveParams()
{
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    QnKvPairList params;

    foreach(const QnParam& param, getResourceParamList().list())
    {
        if (param.domain() == QnDomainDatabase)
            params << QnKvPair(param.name(), param.value().toString());
    }

    QnKvPairListsById  outData;
    ec2::ErrorCode rez = conn->getResourceManager()->saveSync(getId(), params, true, &outData);

    if (rez != ec2::ErrorCode::ok) {
        qCritical() << Q_FUNC_INFO << ": can't save resource params to Enterprise Controller. Resource physicalId: "
            << getPhysicalId() << ". Description: " << ec2::toString(rez);
    }
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
        if (!hasStatusFlags(HasIssuesFlag)) {
            addStatusFlags(HasIssuesFlag);
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

    if (m_issueTimes.empty() && hasStatusFlags(HasIssuesFlag)) {
        removeStatusFlags(HasIssuesFlag);
        lock.unlock();
        saveAsync();
    }
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

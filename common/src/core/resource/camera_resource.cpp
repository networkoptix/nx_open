#include "camera_resource.h"

#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>

#include <nx_ec/data/api_camera_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>

#include <utils/common/model_functions.h>
#include <utils/common/util.h>
#include <utils/math/math.h>

static const float MAX_EPS = 0.01f;
static const int MAX_ISSUE_CNT = 3; // max camera issues during a period.
static const qint64 ISSUE_KEEP_TIMEOUT_MS = 1000 * 60;
static const qint64 UPDATE_BITRATE_TIMEOUT_DAYS = 7;

QnVirtualCameraResource::QnVirtualCameraResource():
    m_dtsFactory(0),
    m_issueCounter(0),
    m_lastIssueTimer()
{}

QString QnVirtualCameraResource::getUniqueId() const
{
    return getPhysicalId();
}

QString QnVirtualCameraResource::toSearchString() const
{
    QString result;
    QTextStream(&result) << QnNetworkResource::toSearchString() << " " << getModel() << " " << getFirmware() << " " << getVendor(); //TODO: #Elric evil!
    return result;
}

QnPhysicalCameraResource::QnPhysicalCameraResource():
    QnVirtualCameraResource(),
    m_channelNumber(0)
{
    setFlags(Qn::local_live_cam);
}

float QnPhysicalCameraResource::rawSuggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const
{
    float lowEnd = 0.1f;
    float hiEnd = 1.0f;

    float qualityFactor = lowEnd + (hiEnd - lowEnd) * (quality - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);

    float resolutionFactor = 0.009f * pow(resolution.width() * resolution.height(), 0.7f);

    float frameRateFactor = fps/1.0f;

    float result = qualityFactor*frameRateFactor * resolutionFactor;

    return qMax(192.0, result);
}

int QnPhysicalCameraResource::suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const
{
    auto result = rawSuggestBitrateKbps(quality, resolution, fps);

    if (bitratePerGopType() != Qn::BPG_None)
        result = result * (30.0 / (qreal)fps);

    return (int) result;
}

int QnPhysicalCameraResource::getChannel() const
{
    QnMutexLocker lock( &m_mutex );
    return m_channelNumber;
}

void QnPhysicalCameraResource::setUrl(const QString &urlStr)
{
    QnVirtualCameraResource::setUrl( urlStr ); /* This call emits, so we should not invoke it under lock. */

    QnMutexLocker lock( &m_mutex );
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
    // SD NTCS/PAL resolutions have non standart SAR. fix it
    else if (resolution.width() == 960 && (resolution.height() == 480 || resolution.height() == 576))
        result = float(16.0 / 9.0);
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

bool QnPhysicalCameraResource::saveMediaStreamInfoIfNeeded( const CameraMediaStreams& streams )
{
    bool rez = false;
    for (const auto& streamInfo: streams.streams)
        rez |= saveMediaStreamInfoIfNeeded(streamInfo);
    return rez;
}

bool QnPhysicalCameraResource::saveBitrateIfNeeded( const CameraBitrateInfo& bitrateInfo )
{
    auto bitrateInfos = QJson::deserialized<CameraBitrates>(
        getProperty(Qn::CAMERA_BITRATE_INFO_LIST_PARAM_NAME).toLatin1() );

    auto it = std::find_if(bitrateInfos.streams.begin(), bitrateInfos.streams.end(),
                           [&](const CameraBitrateInfo& info)
                           { return info.encoderIndex == bitrateInfo.encoderIndex; });

    if (it != bitrateInfos.streams.end())
    {
        const auto time = QDateTime::fromString(bitrateInfo.timestamp, Qt::ISODate);
        const auto lastTime = QDateTime::fromString(it->timestamp, Qt::ISODate);

        // Generally update should not happen more often than once per
        // UPDATE_BITRATE_TIMEOUT_DAYS
        if (lastTime.isValid() && lastTime < time &&
            lastTime.addDays(UPDATE_BITRATE_TIMEOUT_DAYS) > time)
        {
            // If camera got configured we shell update anyway
            bool isGotConfigured = bitrateInfo.isConfigured &&
                it->isConfigured != bitrateInfo.isConfigured;

            if (!isGotConfigured)
                return false;
        }

        // override old data
        *it = bitrateInfo;
    }
    else
    {
        // add new record
        bitrateInfos.streams.push_back(std::move(bitrateInfo));
    }

    setProperty(Qn::CAMERA_BITRATE_INFO_LIST_PARAM_NAME,
                QString::fromUtf8(QJson::serialized(bitrateInfos)));

    return true;
}

bool isParamsCompatible(const CameraMediaStreamInfo& newParams, const CameraMediaStreamInfo& oldParams)
{
    if (newParams.codec != oldParams.codec)
        return false;
    bool streamParamsMatched = newParams.customStreamParams == oldParams.customStreamParams ||
         (newParams.customStreamParams.empty() && !oldParams.customStreamParams.empty());
    bool resolutionMatched = newParams.resolution == oldParams.resolution ||
         ((newParams.resolution == CameraMediaStreamInfo::anyResolution) && (oldParams.resolution != CameraMediaStreamInfo::anyResolution));
    return streamParamsMatched && resolutionMatched;
}

#if !defined(EDGE_SERVER) && !defined(__arm__)
#define TRANSCODING_AVAILABLE
static const bool transcodingAvailable = true;
#else
static const bool transcodingAvailable = false;
#endif

bool QnPhysicalCameraResource::saveMediaStreamInfoIfNeeded( const CameraMediaStreamInfo& mediaStreamInfo )
{
    //saving hasDualStreaming flag before locking mutex
    const auto hasDualStreamingLocal = hasDualStreaming2();

    //TODO #ak remove m_mediaStreamsMutex lock, use resource mutex
    QnMutexLocker lk( &m_mediaStreamsMutex );

    //get saved stream info with index encoderIndex
    const QString& mediaStreamsStr = getProperty( Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME );
    CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>( mediaStreamsStr.toLatin1() );

    const bool isTranscodingAllowedByCurrentMediaStreamsParam = std::find_if(
        supportedMediaStreams.streams.begin(),
        supportedMediaStreams.streams.end(),
        []( const CameraMediaStreamInfo& mediaInfo ) {
            return mediaInfo.transcodingRequired;
        } ) != supportedMediaStreams.streams.end();

    bool needUpdateStreamInfo = true;
    if( isTranscodingAllowedByCurrentMediaStreamsParam == transcodingAvailable )
    {
        //checking if stream info has been changed
        for( auto it = supportedMediaStreams.streams.begin();
            it != supportedMediaStreams.streams.end();
            ++it )
        {
            if( it->encoderIndex == mediaStreamInfo.encoderIndex )
            {
                if( *it == mediaStreamInfo)
                    needUpdateStreamInfo = false;
                //if new media stream info does not contain resolution, preferring existing one
                if (isParamsCompatible(mediaStreamInfo, *it))
                    needUpdateStreamInfo = false;   //stream info has not been changed
                break;
            }
        }
    }
    //else
    //    we have to update information about transcoding availability anyway

    bool modified = false;
    if (needUpdateStreamInfo)
    {
        //removing stream with same encoder index as mediaStreamInfo
        QString previouslySavedResolution;
        supportedMediaStreams.streams.erase(
            std::remove_if(
                supportedMediaStreams.streams.begin(),
                supportedMediaStreams.streams.end(),
                [&mediaStreamInfo, &previouslySavedResolution]( CameraMediaStreamInfo& mediaInfo ) {
                    if( mediaInfo.encoderIndex == mediaStreamInfo.encoderIndex )
                    {
                        previouslySavedResolution = std::move(mediaInfo.resolution);
                        return true;
                    }
                    return false;
                } ),
            supportedMediaStreams.streams.end() );

        CameraMediaStreamInfo newMediaStreamInfo = mediaStreamInfo; //have to copy it anyway to save to supportedMediaStreams.streams
        if( !previouslySavedResolution.isEmpty() &&
            newMediaStreamInfo.resolution == CameraMediaStreamInfo::anyResolution )
        {
            newMediaStreamInfo.resolution = std::move(previouslySavedResolution);
        }
        //saving new stream info
        supportedMediaStreams.streams.push_back(std::move(newMediaStreamInfo));

        modified = true;
    }

    if (!hasDualStreamingLocal)
    {
        //checking whether second stream has disappeared
        auto secondStreamIter = std::find_if(
            supportedMediaStreams.streams.begin(),
            supportedMediaStreams.streams.end(),
            [](const CameraMediaStreamInfo& mediaInfo) {
                return mediaInfo.encoderIndex == CameraMediaStreamInfo::SECONDARY_STREAM_INDEX;
            });
        if (secondStreamIter != supportedMediaStreams.streams.end())
        {
            supportedMediaStreams.streams.erase(secondStreamIter);
            modified = true;
        }
    }

    if (!modified)
        return false;

    //removing non-native streams (they will be re-generated)
    supportedMediaStreams.streams.erase(
        std::remove_if(
            supportedMediaStreams.streams.begin(),
            supportedMediaStreams.streams.end(),
            []( const CameraMediaStreamInfo& mediaStreamInfo ) -> bool {
                return mediaStreamInfo.transcodingRequired;
            } ),
        supportedMediaStreams.streams.end() );

    saveResolutionList( supportedMediaStreams );

    return true;
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
        it->transports.clear();

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

         //TODO: #GDM change to bool transcodingAvailable variable check
#ifdef TRANSCODING_AVAILABLE
    static const char* WEBM_TRANSPORT_NAME = "webm";

    CameraMediaStreamInfo transcodedStream;
    //any resolution is supported
    transcodedStream.transports.push_back( QLatin1String(RTSP_TRANSPORT_NAME) );
    transcodedStream.transports.push_back( QLatin1String(MJPEG_TRANSPORT_NAME) );
    transcodedStream.transports.push_back( QLatin1String(WEBM_TRANSPORT_NAME) );
    transcodedStream.transcodingRequired = true;
    fullStreamList.streams.push_back( transcodedStream );
#endif

    //saving fullStreamList;
    QByteArray serializedStreams = QJson::serialized( fullStreamList );
    setProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME, QString::fromUtf8(serializedStreams));
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

int QnVirtualCameraResource::saveAsync()
{
    ec2::ApiCameraData apiCamera;
    fromResourceToApi(toSharedPointer(this), apiCamera);

    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    return conn->getCameraManager(Qn::kDefaultUserAccess)->addCamera(apiCamera, this, []{});
}

void QnVirtualCameraResource::issueOccured() {
    bool tooManyIssues = false;
    {
        /* Calculate how many issues have occurred during last check period. */
        QnMutexLocker lock( &m_mutex );
        m_issueCounter++;
        tooManyIssues = m_issueCounter >= MAX_ISSUE_CNT;
        m_lastIssueTimer.restart();
    }
    if (tooManyIssues && !hasStatusFlags(Qn::CSF_HasIssuesFlag)) {
        addStatusFlags(Qn::CSF_HasIssuesFlag);
        saveAsync();
    }
}

void QnVirtualCameraResource::cleanCameraIssues() {
    {
        /* Check if no issues occurred during last check period. */
        QnMutexLocker lock( &m_mutex );
        if (!m_lastIssueTimer.hasExpired(issuesTimeoutMs()))
            return;
        m_issueCounter = 0;
    }
    if (hasStatusFlags(Qn::CSF_HasIssuesFlag)) {
        removeStatusFlags(Qn::CSF_HasIssuesFlag);
        saveAsync();
    }
}

int QnVirtualCameraResource::issuesTimeoutMs() {
    return ISSUE_KEEP_TIMEOUT_MS;
}

CameraMediaStreams QnVirtualCameraResource::mediaStreams() const
{
    const QString& mediaStreamsStr = getProperty(Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME);
    CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>(mediaStreamsStr.toLatin1());
    return supportedMediaStreams;
}

const QLatin1String CameraMediaStreamInfo::anyResolution( "*" );

QString CameraMediaStreamInfo::resolutionToString( const QSize& resolution )
{
    if ( !resolution.isValid() )
        return anyResolution;

    return QString::fromLatin1("%1x%2").arg(resolution.width()).arg(resolution.height());
}

bool CameraMediaStreamInfo::operator==( const CameraMediaStreamInfo& rhs ) const
{
    return transcodingRequired == rhs.transcodingRequired
        && codec == rhs.codec
        && encoderIndex == rhs.encoderIndex
        && resolution == rhs.resolution
        && transports == rhs.transports
        && customStreamParams == rhs.customStreamParams;
}

bool CameraMediaStreamInfo::operator!=( const CameraMediaStreamInfo& rhs ) const
{
    return !( *this == rhs );
}

QSize CameraMediaStreamInfo::getResolution() const
{
    QStringList tmp = resolution.split(L'x');
    if (tmp.size() == 2)
        return QSize(tmp[0].toInt(), tmp[1].toInt());
    else
        return QSize();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( \
        (CameraMediaStreamInfo)(CameraMediaStreams) \
        (CameraBitrateInfo)(CameraBitrates), \
        (json), _Fields )

#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/datapacket/media_data_packet.h"
#include "core/resource/camera_resource.h"
#include "core/dataprovider/cpull_media_stream_provider.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"
#include "utils/common/synctime.h"
#include <deque>
#include "core/dataprovider/live_stream_provider.h"

static const qint64 CAMERA_UPDATE_INTERNVAL = 3600 * 1000000ll;
static const qint64 KEEP_IFRAMES_INTERVAL = 1000000ll * 80;
static const qint64 KEEP_IFRAMES_DISTANCE = 1000000ll * 5;
static const qint64 GET_FRAME_MAX_TIME = 1000000ll * 15;
static unsigned int MEDIA_CACHE_SIZE_MILLIS = 10000;

// ------------------------------ QnVideoCameraGopKeeper --------------------------------

class QnVideoCameraGopKeeper: public QnResourceConsumer, public QnAbstractDataConsumer
{
public:
    virtual void beforeDisconnectFromResource();
    QnVideoCameraGopKeeper(QnVideoCamera* camera, const QnResourcePtr &resource,  QnServer::ChunksCatalog catalog);
    virtual ~QnVideoCameraGopKeeper();
    QnAbstractMediaStreamDataProvider* getLiveReader();

    int copyLastGop(qint64 skipTime, CLDataQueue& dstQueue, int cseq);

    // QnAbstractDataConsumer
    virtual bool canAcceptData() const;
    virtual void putData(QnAbstractDataPacketPtr data);
    virtual bool processData(QnAbstractDataPacketPtr data);

    //QnMediaContextPtr getVideoCodecContext();
    //QnMediaContextPtr getAudioCodecContext();
    QnConstCompressedVideoDataPtr getLastVideoFrame() const;
    QnConstCompressedVideoDataPtr GetIFrameByTime(qint64 time, bool iFrameAfterTime) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame() const;
    void updateCameraActivity();
private:
    mutable QMutex m_queueMtx;
    int m_lastKeyFrameChannel;
    QnConstCompressedAudioDataPtr m_lastAudioData;
    QnConstCompressedVideoDataPtr m_lastKeyFrame;
    std::deque<QnConstCompressedVideoDataPtr> m_lastKeyFrames;
    int m_gotIFramesMask;
    int m_allChannelsMask;
    bool m_isSecondaryStream;
    QnVideoCamera* m_camera;
    bool m_activityStarted;
    qint64 m_activityStartTime;
    QnServer::ChunksCatalog m_catalog;
    qint64 m_nextMinTryTime;
};

QnVideoCameraGopKeeper::QnVideoCameraGopKeeper(QnVideoCamera* camera, const QnResourcePtr &resource, QnServer::ChunksCatalog catalog): 
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(100),
    m_lastKeyFrameChannel(0),
    m_gotIFramesMask(0),
    m_allChannelsMask(0),
    m_camera(camera),
    m_activityStarted(false),
    m_activityStartTime(0),
    m_catalog(catalog),
    m_nextMinTryTime(0)
{
    QnConstResourceVideoLayoutPtr layout = (qSharedPointerDynamicCast<QnMediaResource>(resource))->getVideoLayout();
    m_allChannelsMask = (1 << layout->channelCount()) - 1;
}

QnVideoCameraGopKeeper::~QnVideoCameraGopKeeper()
{
    stop();
}

void QnVideoCameraGopKeeper::beforeDisconnectFromResource()
{
    pleaseStop();
}

bool QnVideoCameraGopKeeper::canAcceptData() const
{
    return true;
}

bool channelCheckFunctor(const QnAbstractDataPacketPtr& data, QVariant channelNumber)
{
    quint32 ch = channelNumber.toUInt();
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    return media && media->channelNumber == ch;
}

void QnVideoCameraGopKeeper::putData(QnAbstractDataPacketPtr nonConstData)
{
    QnAbstractDataPacketPtr data = nonConstData;

    QnConstCompressedVideoDataPtr video = qSharedPointerDynamicCast<const QnCompressedVideoData>(data);
    QnConstCompressedAudioDataPtr audio = qSharedPointerDynamicCast<const QnCompressedAudioData>(data);

    QMutexLocker lock(&m_queueMtx);
    if (video)
    {
        if (video->flags & AV_PKT_FLAG_KEY)
        {
            if (m_gotIFramesMask == m_allChannelsMask)
            {
                m_dataQueue.clear();
                m_gotIFramesMask = 0;
            }
            m_gotIFramesMask |= 1 << video->channelNumber;
            m_lastKeyFrame = video;
            if (m_lastKeyFrames.empty() || m_lastKeyFrames.back()->timestamp <= video->timestamp - KEEP_IFRAMES_DISTANCE)
                m_lastKeyFrames.push_back(video);
            qint64 removeThreshold = video->timestamp - KEEP_IFRAMES_INTERVAL;
            while (!m_lastKeyFrames.empty() && m_lastKeyFrames.front()->timestamp < removeThreshold)
                m_lastKeyFrames.pop_front();
        }

        if (m_dataQueue.size() < m_dataQueue.maxSize()) {
            video.constCast<QnCompressedVideoData>()->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            QnAbstractDataConsumer::putData(nonConstData);
        }
    }
    else if (audio) {
        m_lastAudioData = audio;
    }
}

bool QnVideoCameraGopKeeper::processData(QnAbstractDataPacketPtr /*data*/)
{
    return true;
}

int QnVideoCameraGopKeeper::copyLastGop(qint64 skipTime, CLDataQueue& dstQueue, int cseq)
{
    int rez = 0;
    QMutexLocker lock(&m_queueMtx);
    for (int i = 0; i < m_dataQueue.size(); ++i)
    {
        QnConstAbstractDataPacketPtr data = m_dataQueue.at(i);
        QnConstCompressedVideoDataPtr video = qSharedPointerDynamicCast<const QnCompressedVideoData>(data);
        if (video)
        {
            QnCompressedVideoData* newData = video->clone();
            if (skipTime && video->timestamp <= skipTime)
                newData->flags |= QnAbstractMediaData::MediaFlags_Ignore;
            newData->opaque = cseq;
            dstQueue.push(QnAbstractMediaDataPtr(newData));
        }
        else { 
            dstQueue.push(data.constCast<QnAbstractDataPacket>());    //TODO: #ak remove const_cast
        }
        rez++;
    }
    return rez;
}


QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::GetIFrameByTime(qint64 time, bool iFrameAfterTime) const
{
    QnConstCompressedVideoDataPtr result;

    QMutexLocker lock(&m_queueMtx);
    if (m_lastKeyFrames.empty() || 
        m_lastKeyFrames.front()->timestamp > time + KEEP_IFRAMES_DISTANCE ||
        m_lastKeyFrames.back()->timestamp < time - KEEP_IFRAMES_DISTANCE)
    {
        return result;
    }

    for (int i = 0; i < (int)m_lastKeyFrames.size(); ++i)
    {
        if (m_lastKeyFrames[i]->timestamp >= time) {
            if (iFrameAfterTime || m_lastKeyFrames[i]->timestamp == time || i == 0)
                return m_lastKeyFrames[i];
            else
                return m_lastKeyFrames[i-1];
        }
    }
    if (iFrameAfterTime || m_lastKeyFrames.empty())
        return m_lastKeyFrame;
    else
        return m_lastKeyFrames.back();
}


QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::getLastVideoFrame() const
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastKeyFrame;
}

QnConstCompressedAudioDataPtr QnVideoCameraGopKeeper::getLastAudioFrame() const
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastAudioData;
}

void QnVideoCameraGopKeeper::updateCameraActivity()
{
    qint64 usecTime = getUsecTimer();
    if (!m_resource->hasFlags(QnResource::foreigner) && m_resource->isInitialized() &&
       (!m_lastKeyFrame || qnSyncTime->currentUSecsSinceEpoch() - m_lastKeyFrame->timestamp > CAMERA_UPDATE_INTERNVAL))
    {
        if (!m_activityStarted && usecTime > m_nextMinTryTime) {
            m_activityStarted = true;
            m_activityStartTime = usecTime;
            m_nextMinTryTime = usecTime + (rand()%100 + 60*5) * 1000000ll;
            m_camera->inUse(this);
            QnLiveStreamProviderPtr provider = m_camera->getLiveReader(m_catalog);
            if (provider)
                provider->startIfNotRunning();
        }
        else if (m_activityStarted && usecTime - m_activityStartTime > GET_FRAME_MAX_TIME )
        {
            m_activityStarted = false;
            m_camera->notInUse(this);
        }
    }
    else {
        if (m_activityStarted) {
            m_activityStarted = false;
            m_camera->notInUse(this);
        }
    }
    
}

// --------------- QnVideoCamera ----------------------------

QnVideoCamera::QnVideoCamera(QnResourcePtr resource): m_resource(resource)
{
    m_primaryGopKeeper = 0;
    m_secondaryGopKeeper = 0;
}

void QnVideoCamera::beforeStop()
{
    if (m_primaryGopKeeper)
        m_primaryGopKeeper->beforeDisconnectFromResource();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->beforeDisconnectFromResource();

    if (m_primaryGopKeeper)
        m_primaryGopKeeper->disconnectFromResource();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->disconnectFromResource();

    if (m_primaryReader) {
        m_primaryReader->removeDataProcessor(m_primaryGopKeeper);
        m_primaryReader->pleaseStop();
    }

    if (m_secondaryReader) {
        m_secondaryReader->removeDataProcessor(m_secondaryGopKeeper);
        m_secondaryReader->pleaseStop();
    }
}

void QnVideoCamera::stop()
{
    if (m_primaryReader)
        m_primaryReader->stop();
    if (m_secondaryReader)
        m_secondaryReader->stop();
}


QnVideoCamera::~QnVideoCamera()
{
    beforeStop();
    stop();
    delete m_primaryGopKeeper;
    delete m_secondaryGopKeeper;
}

void QnVideoCamera::at_camera_resourceChanged()
{
	QMutexLocker lock(&m_getReaderMutex);

	QnSecurityCamResourcePtr cameraResource = m_resource.dynamicCast<QnSecurityCamResource>();
	if ( cameraResource )
	{
		if ( !cameraResource->hasDualStreaming2() && m_secondaryReader )
		{
			if ( m_secondaryReader->isRunning() )
				m_secondaryReader->pleaseStop();
		}
	}
}

void QnVideoCamera::createReader(QnServer::ChunksCatalog catalog)
{
    const bool primaryLiveStream = catalog == QnServer::HiQualityCatalog;
    const QnResource::ConnectionRole role = primaryLiveStream ? QnResource::Role_LiveVideo : QnResource::Role_SecondaryLiveVideo;

	QnSecurityCamResourcePtr cameraResource = m_resource.dynamicCast<QnSecurityCamResource>();
	
    QnLiveStreamProviderPtr &reader = primaryLiveStream ? m_primaryReader : m_secondaryReader;
    if (reader == 0)
    {
		QnAbstractStreamDataProvider* dataProvider = NULL;
		if ( primaryLiveStream || (cameraResource && cameraResource->hasDualStreaming2()) )
			dataProvider = m_resource->createDataProvider(role);

		if ( dataProvider )
		{
			reader = QnLiveStreamProviderPtr(dynamic_cast<QnLiveStreamProvider*>(dataProvider));
			if (reader == 0)
			{
				delete dataProvider;
			} else
			{
				if ( role ==  QnResource::Role_LiveVideo )
					connect(reader->getResource().data(), SIGNAL(resourceChanged(const QnResourcePtr &)), this, SLOT(at_camera_resourceChanged()), Qt::DirectConnection);
					
				QnVideoCameraGopKeeper* gopKeeper = new QnVideoCameraGopKeeper(this, m_resource, catalog);
				if (primaryLiveStream)
					m_primaryGopKeeper = gopKeeper;
				else
					m_secondaryGopKeeper = gopKeeper;
				reader->addDataProcessor(gopKeeper);
			}			
		}
    }
}

QnLiveStreamProviderPtr QnVideoCamera::getLiveReader(QnServer::ChunksCatalog catalog)
{
    QMutexLocker lock(&m_getReaderMutex);

    if (!m_resource->hasFlags(QnResource::foreigner) && m_resource->isInitialized()) 
    {
        if( (catalog == QnServer::HiQualityCatalog && m_primaryReader == 0) ||
            (catalog == QnServer::LowQualityCatalog && m_secondaryReader == 0) )
        {
            createReader(catalog);
        }
    }
	QnSecurityCamResourcePtr cameraResource = m_resource.dynamicCast<QnSecurityCamResource>();
	if ( cameraResource && !cameraResource->hasDualStreaming2() && catalog == QnServer::LowQualityCatalog )
	{
		return QnLiveStreamProviderPtr();		
	}
    return catalog == QnServer::HiQualityCatalog ? m_primaryReader : m_secondaryReader;
}

int QnVideoCamera::copyLastGop(bool primaryLiveStream, qint64 skipTime, CLDataQueue& dstQueue, int cseq)
{
    if (primaryLiveStream)
        return m_primaryGopKeeper->copyLastGop(skipTime, dstQueue, cseq);
    else
        return m_secondaryGopKeeper->copyLastGop(skipTime, dstQueue, cseq);
}

/*
QnMediaContextPtr QnVideoCamera::getVideoCodecContext(bool primaryLiveStream)
{
    if (primaryLiveStream)
        return m_primaryGopKeeper->getVideoCodecContext();
    else
        return m_secondaryGopKeeper->getVideoCodecContext();
}

QnMediaContextPtr QnVideoCamera::getAudioCodecContext(bool primaryLiveStream)
{
    if (primaryLiveStream)
        return m_primaryGopKeeper->getAudioCodecContext();
    else
        return m_secondaryGopKeeper->getAudioCodecContext();
}
*/

QnConstCompressedVideoDataPtr QnVideoCamera::getFrameByTime(bool primaryLiveStream, qint64 time, bool iFrameAfterTime) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->GetIFrameByTime(time, iFrameAfterTime);
    else
        return QnCompressedVideoDataPtr();
}

QnConstCompressedVideoDataPtr QnVideoCamera::getLastVideoFrame(bool primaryLiveStream) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->getLastVideoFrame();
    else
        return QnCompressedVideoDataPtr();
}

QnConstCompressedAudioDataPtr QnVideoCamera::getLastAudioFrame(bool primaryLiveStream) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->getLastAudioFrame();
    else
        return QnCompressedAudioDataPtr();
}

void QnVideoCamera::inUse(void* user)
{
    QMutexLocker lock(&m_getReaderMutex);
    m_cameraUsers << user;
}

void QnVideoCamera::notInUse(void* user)
{
    QMutexLocker lock(&m_getReaderMutex);
    m_cameraUsers.remove(user);
}

bool QnVideoCamera::isSomeActivity() const
{
    return !m_cameraUsers.isEmpty() && !m_resource->hasFlags(QnResource::foreigner);
}

void QnVideoCamera::updateActivity()
{
    if (m_primaryGopKeeper)
        m_primaryGopKeeper->updateCameraActivity();
    if (m_secondaryGopKeeper)
        m_secondaryGopKeeper->updateCameraActivity();
    stopIfNoActivity();
}

void QnVideoCamera::stopIfNoActivity()
{
    QMutexLocker lock(&m_getReaderMutex);
    if (isSomeActivity())
        return;

    bool needStopPrimary = m_primaryReader && m_primaryReader->isRunning();
    bool needStopSecondary = m_secondaryReader && m_secondaryReader->isRunning();

    if (needStopPrimary)
        m_primaryReader->pleaseStop();
    if (needStopSecondary)
        m_secondaryReader->pleaseStop();
}

const MediaStreamCache* QnVideoCamera::liveCache( MediaQuality streamQuality ) const
{
    return streamQuality < m_liveCache.size()
        ? m_liveCache[streamQuality].get()
        : nullptr;
}

MediaStreamCache* QnVideoCamera::liveCache( MediaQuality streamQuality )
{
    return streamQuality < m_liveCache.size()
        ? m_liveCache[streamQuality].get()
        : nullptr;
}

QSharedPointer<nx_hls::HLSLivePlaylistManager> QnVideoCamera::hlsLivePlaylistManager( MediaQuality streamQuality ) const
{
    return streamQuality < m_hlsLivePlaylistManager.size()
        ? m_hlsLivePlaylistManager[streamQuality]
        : QSharedPointer<nx_hls::HLSLivePlaylistManager>();
}

//!Starts caching live stream, if not started
/*!
    \return true, if started, false if failed to start
*/
bool QnVideoCamera::ensureLiveCacheStarted( MediaQuality streamQuality, qint64 targetDurationUSec )
{
    assert( streamQuality == MEDIA_Quality_High || streamQuality == MEDIA_Quality_Low );

    if( streamQuality == MEDIA_Quality_High )
    {
        if( !m_primaryReader )
            getLiveReader( QnServer::HiQualityCatalog );
        if( !m_primaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_primaryReader, targetDurationUSec );
    }

    if( streamQuality == MEDIA_Quality_Low )
    {
        if( !m_secondaryReader )
            getLiveReader( QnServer::LowQualityCatalog );
        if( !m_secondaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_secondaryReader, targetDurationUSec );
    }

    return false;
}

//!Starts caching live stream, if not started
/*!
    \return true, if started, false if failed to start
*/
bool QnVideoCamera::ensureLiveCacheStarted(
    MediaQuality streamQuality,
    QnAbstractMediaStreamDataProviderPtr primaryReader,
    qint64 targetDurationUSec )
{
    //ensuring that vectors will not take much memory
    static_assert(
        ((MEDIA_Quality_High > MEDIA_Quality_Low ? MEDIA_Quality_High : MEDIA_Quality_Low) + 1) < 16,
        "MediaQuality enum suddenly contains too large values: consider changing QnVideoCamera::m_liveCache, QnVideoCamera::m_mediaIndexes, QnVideoCamera::m_hlsLivePlaylistManager type" );  

    m_liveCache.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    m_mediaIndexes.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    m_hlsLivePlaylistManager.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );

    if( m_liveCache[streamQuality].get() )
        return true;
    if( !primaryReader )
        return false;

    m_mediaIndexes[streamQuality].reset( new MediaIndex() );
    m_liveCache[streamQuality].reset( new MediaStreamCache( MEDIA_CACHE_SIZE_MILLIS, m_mediaIndexes[streamQuality].get() ) );
    m_hlsLivePlaylistManager[streamQuality] = QSharedPointer<nx_hls::HLSLivePlaylistManager>(
        new nx_hls::HLSLivePlaylistManager(
            m_liveCache[streamQuality].get(),
            m_mediaIndexes[streamQuality].get(),
            targetDurationUSec ) );
    //connecting live cache to reader
    primaryReader->addDataProcessor( m_liveCache[streamQuality].get() );
    m_cameraUsers << this;
    return true;
}

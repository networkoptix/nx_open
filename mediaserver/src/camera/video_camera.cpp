#include "video_camera.h"

#include <deque>

#include <utils/common/synctime.h>
#include <utils/common/util.h> /* For getUsecTimer. */
#include <utils/media/frame_info.h>
#include <utils/media/media_stream_cache.h>
#include <utils/memory/cyclic_allocator.h>

#include "core/dataprovider/media_streamdataprovider.h"
#include "core/datapacket/media_data_packet.h"
#include "core/dataprovider/cpull_media_stream_provider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/resource/camera_resource.h"

#include "decoders/video/ffmpeg.h"
#include "media_server/settings.h"


static const qint64 CAMERA_UPDATE_INTERNVAL = 3600 * 1000000ll;
static const qint64 KEEP_IFRAMES_INTERVAL = 1000000ll * 80;
static const qint64 KEEP_IFRAMES_DISTANCE = 1000000ll * 5;
static const qint64 GET_FRAME_MAX_TIME = 1000000ll * 15;
static unsigned int MEDIA_CACHE_SIZE_MILLIS = 10*1000;
static const quint64 MSEC_PER_SEC = 1000;

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
    virtual void putData(const QnAbstractDataPacketPtr& data);
    virtual bool processData(const QnAbstractDataPacketPtr& data);

    //QnMediaContextPtr getVideoCodecContext();
    //QnMediaContextPtr getAudioCodecContext();
    QnConstCompressedVideoDataPtr getLastVideoFrame(int channel) const;
    QnConstCompressedVideoDataPtr GetIFrameByTime(qint64 time, bool iFrameAfterTime, int channel) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame() const;
    void updateCameraActivity();
    virtual bool needConfigureProvider() const override { return false; }
private:
    mutable QMutex m_queueMtx;
    int m_lastKeyFrameChannel;
    QnConstCompressedAudioDataPtr m_lastAudioData;
    QnConstCompressedVideoDataPtr m_lastKeyFrame[CL_MAX_CHANNELS];
    std::deque<QnConstCompressedVideoDataPtr> m_lastKeyFrames[CL_MAX_CHANNELS];
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

/*!
    using different allocator for stored ket frames, since these key frames can be kept in QnVideoCameraGopKeeper::m_lastKeyFrames 
    for 80 seconds and that can cause huge memory consumption if CyclicAllocator has been used to alloc original frames
*/
static CyclicAllocator gopKeeperKeyFramesAllocator;

void QnVideoCameraGopKeeper::putData(const QnAbstractDataPacketPtr& nonConstData)
{
    QMutexLocker lock(&m_queueMtx);
    if (QnConstCompressedVideoDataPtr video = qSharedPointerDynamicCast<const QnCompressedVideoData>(nonConstData))
    {
        if (video->flags & AV_PKT_FLAG_KEY)
        {
            if (m_gotIFramesMask == m_allChannelsMask)
            {
                m_dataQueue.clear();
                m_gotIFramesMask = 0;
            }
            m_gotIFramesMask |= 1 << video->channelNumber;
            int ch = video->channelNumber;
            m_lastKeyFrame[ch] = video;
            const qint64 removeThreshold = video->timestamp - KEEP_IFRAMES_INTERVAL;
            if (m_lastKeyFrames[ch].empty() || m_lastKeyFrames[ch].back()->timestamp <= video->timestamp - KEEP_IFRAMES_DISTANCE)
                m_lastKeyFrames[ch].push_back(QnCompressedVideoDataPtr(video->clone(&gopKeeperKeyFramesAllocator)));
            while (!m_lastKeyFrames[ch].empty() && m_lastKeyFrames[ch].front()->timestamp < removeThreshold)
                m_lastKeyFrames[ch].pop_front();
        }

        if (m_dataQueue.size() < m_dataQueue.maxSize()) {
            //TODO #ak MUST NOT modify video packet here! It can be used by other threads concurrently and flags value can be undefined in other threads
            static_cast<QnAbstractMediaData*>(nonConstData.data())->flags |= QnAbstractMediaData::MediaFlags_LIVE;
            QnAbstractDataConsumer::putData( nonConstData );
        }
    }
    else if (QnConstCompressedAudioDataPtr audio = qSharedPointerDynamicCast<const QnCompressedAudioData>(nonConstData))
    {
        m_lastAudioData = std::move(audio);
    }
}

bool QnVideoCameraGopKeeper::processData(const QnAbstractDataPacketPtr& /*data*/)
{
    return true;
}

int QnVideoCameraGopKeeper::copyLastGop(qint64 skipTime, CLDataQueue& dstQueue, int cseq)
{
    int rez = 0;
    QMutexLocker lock(&m_queueMtx);
    for (int i = 0; i < m_dataQueue.size(); ++i)
    {
        const QnConstAbstractDataPacketPtr& data = m_dataQueue.atUnsafe(i);
        const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(data.data());
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


QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::GetIFrameByTime(qint64 time, bool iFrameAfterTime, int channel) const
{
    QMutexLocker lock(&m_queueMtx);
    const auto &queue = m_lastKeyFrames[channel];
    if (queue.empty())
        return QnConstCompressedVideoDataPtr(); // no video data
    auto itr = std::lower_bound(queue.begin(), queue.end(), time, [](const QnConstCompressedVideoDataPtr& data, qint64 time) { return data->timestamp < time; } );
    if (itr == queue.end())
        return queue.back();
    if (itr != queue.begin() && (*itr)->timestamp > time && !iFrameAfterTime)
        --itr; // prefer frame before defined time if no exact match
    return *itr;
}


QnConstCompressedVideoDataPtr QnVideoCameraGopKeeper::getLastVideoFrame(int channel) const
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastKeyFrame[channel];
}

QnConstCompressedAudioDataPtr QnVideoCameraGopKeeper::getLastAudioFrame() const
{
    QMutexLocker lock(&m_queueMtx);
    return m_lastAudioData;
}

void QnVideoCameraGopKeeper::updateCameraActivity()
{
    qint64 usecTime = getUsecTimer();
    qint64 lastKeyTime = AV_NOPTS_VALUE;
    {
        QMutexLocker lock(&m_queueMtx);
        if (m_lastKeyFrame[0])
            lastKeyTime = m_lastKeyFrame[0]->timestamp;
    }
    if (!m_resource->hasFlags(Qn::foreigner) && m_resource->isInitialized() &&
       (lastKeyTime == AV_NOPTS_VALUE || qnSyncTime->currentUSecsSinceEpoch() - lastKeyTime > CAMERA_UPDATE_INTERNVAL) &&
       m_catalog == QnServer::HiQualityCatalog)
    {
        if (m_nextMinTryTime == 0)
            m_nextMinTryTime = usecTime + (rand()%5000 + 5000) * 1000ll; // get first screenshot after minor delay

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

QnVideoCamera::QnVideoCamera(const QnResourcePtr& resource)
:
    m_resource(resource),
    m_hlsInactivityPeriodMS( MSSettings::roSettings()->value( nx_ms_conf::HLS_INACTIVITY_PERIOD, nx_ms_conf::DEFAULT_HLS_INACTIVITY_PERIOD ).toInt() * MSEC_PER_SEC )
{
    m_primaryGopKeeper = 0;
    m_secondaryGopKeeper = 0;

    //ensuring that vectors will not take much memory
    static_assert(
        ((MEDIA_Quality_High > MEDIA_Quality_Low ? MEDIA_Quality_High : MEDIA_Quality_Low) + 1) < 16,
        "MediaQuality enum suddenly contains too large values: consider changing QnVideoCamera::m_liveCache, QnVideoCamera::m_hlsLivePlaylistManager type" );  

    m_liveCache.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
    m_hlsLivePlaylistManager.resize( std::max<>( MEDIA_Quality_High, MEDIA_Quality_Low ) + 1 );
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

	const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
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
    const Qn::ConnectionRole role = primaryLiveStream ? Qn::CR_LiveVideo : Qn::CR_SecondaryLiveVideo;

	const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
	
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
                reader->setOwner(this);
				if ( role ==  Qn::CR_LiveVideo )
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

QnLiveStreamProviderPtr QnVideoCamera::getPrimaryReader()
{
    return getLiveReader(QnServer::HiQualityCatalog);
}

QnLiveStreamProviderPtr QnVideoCamera::getSecondaryReader()
{
    return getLiveReader(QnServer::LowQualityCatalog);
}

QnLiveStreamProviderPtr QnVideoCamera::getLiveReader(QnServer::ChunksCatalog catalog)
{
    QMutexLocker lock(&m_getReaderMutex);
    return getLiveReaderNonSafe( catalog );
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

QnConstCompressedVideoDataPtr QnVideoCamera::getFrameByTime(bool primaryLiveStream, qint64 time, bool iFrameAfterTime, int channel) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->GetIFrameByTime(time, iFrameAfterTime, channel);
    else
        return QnCompressedVideoDataPtr();
}

QnConstCompressedVideoDataPtr QnVideoCamera::getLastVideoFrame(bool primaryLiveStream, int channel) const
{
    QnVideoCameraGopKeeper* gopKeeper = primaryLiveStream ? m_primaryGopKeeper : m_secondaryGopKeeper;
    if (gopKeeper)
        return gopKeeper->getLastVideoFrame(channel);
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
    return !m_cameraUsers.isEmpty() && !m_resource->hasFlags(Qn::foreigner);
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

    //stopping live cache (used for HLS)
    if( (m_liveCache[MEDIA_Quality_High] || m_liveCache[MEDIA_Quality_Low])     //has live cache ever been started?
        &&
        (!m_liveCache[MEDIA_Quality_High] ||                                    //has hi quality live cache been started?
            (m_hlsLivePlaylistManager[MEDIA_Quality_High].unique() &&           //no one uses playlist
             m_hlsLivePlaylistManager[MEDIA_Quality_High]->inactivityPeriod() > m_hlsInactivityPeriodMS &&  //checking inactivity timer
             m_liveCache[MEDIA_Quality_High]->inactivityPeriod() > m_hlsInactivityPeriodMS))
        &&
        (!m_liveCache[MEDIA_Quality_Low] ||
            (m_hlsLivePlaylistManager[MEDIA_Quality_Low].unique() &&
             m_hlsLivePlaylistManager[MEDIA_Quality_Low]->inactivityPeriod() > m_hlsInactivityPeriodMS &&
             m_liveCache[MEDIA_Quality_Low]->inactivityPeriod() > m_hlsInactivityPeriodMS)) )
    {
        m_cameraUsers.remove(this);

        if( m_liveCache[MEDIA_Quality_High] )
        {
            if( m_primaryReader )
                m_primaryReader->removeDataProcessor( m_liveCache[MEDIA_Quality_High].get() );
            m_hlsLivePlaylistManager[MEDIA_Quality_High].reset();
            m_liveCache[MEDIA_Quality_High].reset();
        }

        if( m_liveCache[MEDIA_Quality_Low] )
        {
            if( m_secondaryReader )
                m_secondaryReader->removeDataProcessor( m_liveCache[MEDIA_Quality_Low].get() );
            m_hlsLivePlaylistManager[MEDIA_Quality_Low].reset();
            m_liveCache[MEDIA_Quality_Low].reset();
        }
    }

    if (isSomeActivity())
        return;

    const bool needStopPrimary = m_primaryReader && m_primaryReader->isRunning();
    const bool needStopSecondary = m_secondaryReader && m_secondaryReader->isRunning();

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

nx_hls::HLSLivePlaylistManagerPtr QnVideoCamera::hlsLivePlaylistManager( MediaQuality streamQuality ) const
{
    return streamQuality < m_hlsLivePlaylistManager.size()
        ? m_hlsLivePlaylistManager[streamQuality]
        : nx_hls::HLSLivePlaylistManagerPtr();
}

//!Starts caching live stream, if not started
/*!
    \return true, if started, false if failed to start
*/
bool QnVideoCamera::ensureLiveCacheStarted( MediaQuality streamQuality, qint64 targetDurationUSec )
{
    QMutexLocker lock(&m_getReaderMutex);
 
    assert( streamQuality == MEDIA_Quality_High || streamQuality == MEDIA_Quality_Low );

    if( streamQuality == MEDIA_Quality_High )
    {
        if( !m_primaryReader )
            getLiveReaderNonSafe( QnServer::HiQualityCatalog );
        if( !m_primaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_primaryReader, targetDurationUSec );
    }

    if( streamQuality == MEDIA_Quality_Low )
    {
        if( !m_secondaryReader )
            getLiveReaderNonSafe( QnServer::LowQualityCatalog );
        if( !m_secondaryReader )
            return false;
        return ensureLiveCacheStarted( streamQuality, m_secondaryReader, targetDurationUSec );
    }

    return false;
}

QnLiveStreamProviderPtr QnVideoCamera::getLiveReaderNonSafe(QnServer::ChunksCatalog catalog)
{
    if( m_resource->hasFlags(Qn::foreigner) )
        return QnLiveStreamProviderPtr();

    if( m_resource->isInitialized() )
    {
        if( (catalog == QnServer::HiQualityCatalog && m_primaryReader == 0) ||
            (catalog == QnServer::LowQualityCatalog && m_secondaryReader == 0) )
        {
            createReader(catalog);
        }
    }
    else
    {
        m_resource->initAsync( true );
    }
	const QnSecurityCamResource* cameraResource = dynamic_cast<QnSecurityCamResource*>(m_resource.data());
	if ( cameraResource && !cameraResource->hasDualStreaming2() && catalog == QnServer::LowQualityCatalog )
		return QnLiveStreamProviderPtr();		
    return catalog == QnServer::HiQualityCatalog ? m_primaryReader : m_secondaryReader;
}

//!Starts caching live stream, if not started
/*!
    \return true, if started, false if failed to start
*/
bool QnVideoCamera::ensureLiveCacheStarted(
    MediaQuality streamQuality,
    const QnLiveStreamProviderPtr& primaryReader,
    qint64 targetDurationUSec )
{
    primaryReader->startIfNotRunning();

    m_cameraUsers.insert(this);

    if( !m_liveCache[streamQuality].get() )
    {
        m_liveCache[streamQuality].reset( new MediaStreamCache( MEDIA_CACHE_SIZE_MILLIS ) );
        m_hlsLivePlaylistManager[streamQuality] = 
            std::make_shared<nx_hls::HLSLivePlaylistManager>(
                m_liveCache[streamQuality].get(),
                targetDurationUSec );
    }
    //connecting live cache to reader
    primaryReader->addDataProcessor( m_liveCache[streamQuality].get() );
    return true;
}

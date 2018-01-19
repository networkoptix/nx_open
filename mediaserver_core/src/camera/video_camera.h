#pragma once

#include <memory>

#include <QScopedPointer>

#include <server/server_globals.h>

#include <nx/streaming/abstract_data_consumer.h>
#include <core/resource/resource_consumer.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <streaming/hls/hls_live_playlist_manager.h>
#include <core/dataprovider/live_stream_provider.h>

#include <nx/api/mediaserver/image_request.h>

#include "camera_fwd.h"


class QnVideoCameraGopKeeper;
class MediaStreamCache;

class QnVideoCamera: public QObject, public QnAbstractVideoCamera
{
    Q_OBJECT

public:
    QnVideoCamera(const QnResourcePtr& resource);
    virtual ~QnVideoCamera();

    QnLiveStreamProviderPtr getLiveReader(QnServer::ChunksCatalog catalog, bool ensureInitialized = true);
    virtual QnLiveStreamProviderPtr getPrimaryReader() override;
    virtual QnLiveStreamProviderPtr getSecondaryReader() override;

    int copyLastGop(
        bool primaryLiveStream,
        qint64 skipTime,
        QnDataPacketQueue& dstQueue,
        int cseq,
        bool iFramesOnly);

    //QnMediaContextPtr getVideoCodecContext(bool primaryLiveStream);
    //QnMediaContextPtr getAudioCodecContext(bool primaryLiveStream);
    QnConstCompressedVideoDataPtr getLastVideoFrame(bool primaryLiveStream, int channel) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame(bool primaryLiveStream) const;

    std::unique_ptr<QnConstDataPacketQueue> getFrameSequenceByTime(
        bool primaryLiveStream,
        qint64 time,
        int channel,
        nx::api::ImageRequest::RoundMethod roundMethod) const;

    Q_SLOT void at_camera_resourceChanged();
    void beforeStop();

    bool isSomeActivity() const;

    /* stop reading from camera if no active DataConsumers left */
    void stopIfNoActivity();

    void updateActivity();

    /* Mark some camera activity (RTSP client connection for example) */
    virtual void inUse(void* user) override;

    /* Unmark some camera activity (RTSP client connection for example) */
    virtual void notInUse(void* user) override;

    //!Returns cache holding several last seconds of media stream
    /*!
        \return Can be NULL
    */
    const MediaStreamCache* liveCache( MediaQuality streamQuality ) const;
    MediaStreamCache* liveCache( MediaQuality streamQuality );

    /*!
        \todo Should remove it from here
    */
    nx_hls::HLSLivePlaylistManagerPtr hlsLivePlaylistManager( MediaQuality streamQuality ) const;

    //!Starts caching live stream, if not started
    /*!
        \return true, if started, false if failed to start
    */
    bool ensureLiveCacheStarted( MediaQuality streamQuality, qint64 targetDurationUSec );
    QnResourcePtr resource() const { return m_resource; }
private:
    void createReader(QnServer::ChunksCatalog catalog);
    void stop();
private:
    QnMutex m_readersMutex;
    QnMutex m_getReaderMutex;
    QnResourcePtr m_resource;
    QnLiveStreamProviderPtr m_primaryReader;
    QnLiveStreamProviderPtr m_secondaryReader;

    QnVideoCameraGopKeeper* m_primaryGopKeeper;
    QnVideoCameraGopKeeper* m_secondaryGopKeeper;
    QSet<void*> m_cameraUsers;
    QnCompressedAudioDataPtr m_lastAudioFrame;
    //!index - is a \a MediaQuality element
    std::vector<std::unique_ptr<MediaStreamCache> > m_liveCache;
    //!index - is a \a MediaQuality element
    std::vector<nx_hls::HLSLivePlaylistManagerPtr> m_hlsLivePlaylistManager;

    const qint64 m_loStreamHlsInactivityPeriodMS;
    const qint64 m_hiStreamHlsInactivityPeriodMS;

    QnLiveStreamProviderPtr getLiveReaderNonSafe(QnServer::ChunksCatalog catalog, bool ensureInitialized);
    void startLiveCacheIfNeeded();
    bool ensureLiveCacheStarted(
        MediaQuality streamQuality,
        const QnLiveStreamProviderPtr& primaryReader,
        qint64 targetDurationUSec );
    QElapsedTimer m_lastActivityTimer;
};

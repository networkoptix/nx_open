#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#include <memory>

#include <QScopedPointer>

#include <server/server_globals.h>

#include <core/dataconsumer/abstract_data_consumer.h>
#include <core/resource/resource_consumer.h>
#include "core/dataprovider/media_streamdataprovider.h"
#include "streaming/hls/hls_live_playlist_manager.h"
#include "core/dataprovider/live_stream_provider.h"

class QnVideoCameraGopKeeper;
class MediaStreamCache;

class QnVideoCamera: public QObject
{
    Q_OBJECT

public:
    QnVideoCamera(QnResourcePtr resource);
    virtual ~QnVideoCamera();
    QnLiveStreamProviderPtr getLiveReader(QnServer::ChunksCatalog catalog);
    int copyLastGop(bool primaryLiveStream, qint64 skipTime, CLDataQueue& dstQueue, int cseq);

    //QnMediaContextPtr getVideoCodecContext(bool primaryLiveStream);
    //QnMediaContextPtr getAudioCodecContext(bool primaryLiveStream);
    QnConstCompressedVideoDataPtr getLastVideoFrame(bool primaryLiveStream) const;
    QnConstCompressedVideoDataPtr getFrameByTime(bool primaryLiveStream, qint64 time, bool iFrameAfterTime) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame(bool primaryLiveStream) const;
	Q_SLOT void at_camera_resourceChanged();
    void beforeStop();

    bool isSomeActivity() const;

    /* stop reading from camera if no active DataConsumers left */
    void stopIfNoActivity();

    void updateActivity();

    /* Mark some camera activity (RTSP client connection for example) */
    void inUse(void* user);

    /* Unmark some camera activity (RTSP client connection for example) */
    void notInUse(void* user);

    //!Returns cache holding several last seconds of media stream
    /*!
        \return Can be NULL
    */
    const MediaStreamCache* liveCache( MediaQuality streamQuality ) const;
    MediaStreamCache* liveCache( MediaQuality streamQuality );

    /*!
        \todo Should remove it from here
    */
    QSharedPointer<nx_hls::HLSLivePlaylistManager> hlsLivePlaylistManager( MediaQuality streamQuality ) const;

    //!Starts caching live stream, if not started
    /*!
        \return true, if started, false if failed to start
    */
    bool ensureLiveCacheStarted( MediaQuality streamQuality, qint64 targetDurationUSec );

private:
    void createReader(QnServer::ChunksCatalog catalog);
    void stop();
private:
    QMutex m_readersMutex;
    QMutex m_getReaderMutex;
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
    std::vector<QSharedPointer<nx_hls::HLSLivePlaylistManager> > m_hlsLivePlaylistManager;

    QnLiveStreamProviderPtr getLiveReaderNonSafe(QnServer::ChunksCatalog catalog);
    bool ensureLiveCacheStarted(
        MediaQuality streamQuality,
        QnLiveStreamProviderPtr primaryReader,
        qint64 targetDurationUSec );
};

#endif // __VIDEO_CAMERA_H__

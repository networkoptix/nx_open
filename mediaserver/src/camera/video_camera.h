#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#include <server/server_globals.h>

#include <core/dataconsumer/abstract_data_consumer.h>
#include <core/resource/resource_consumer.h>
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/dataprovider/live_stream_provider.h"

class QnVideoCameraGopKeeper;

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
};

#endif // __VIDEO_CAMERA_H__

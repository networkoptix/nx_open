#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#include <core/dataconsumer/dataconsumer.h>
#include <core/resource/resource_consumer.h>
#include "core/dataprovider/media_streamdataprovider.h"

class QnVideoCameraGopKeeper;

class QnVideoCamera: public QObject
{
    Q_OBJECT
public:
    QnVideoCamera(QnResourcePtr resource);
    virtual ~QnVideoCamera();
    QnAbstractMediaStreamDataProviderPtr getLiveReader(QnResource::ConnectionRole role);
    int copyLastGop(bool primaryLiveStream, qint64 skipTime, CLDataQueue& dstQueue);

    void beforeStop();

    bool isSomeActivity() const;

    /* stop reading from camera if no active DataConsumers left */
    void stopIfNoActivity();

    /* Mark some camera activity (RTSP client connection for example) */
    void inUse(void* user);

    /* Unmark some camera activity (RTSP client connection for example) */
    void notInUse(void* user);

private:
    void createReader(QnResource::ConnectionRole role);
private:
    QMutex m_readersMutex;
    QMutex m_getReaderMutex;
    QnResourcePtr m_resource;
    QnAbstractMediaStreamDataProviderPtr m_primaryReader;
    QnAbstractMediaStreamDataProviderPtr m_secondaryReader;

    QnVideoCameraGopKeeper* m_primaryGopKeeper;
    QnVideoCameraGopKeeper* m_secondaryGopKeeper;
    QSet<void*> m_cameraUsers;
};

#endif // __VIDEO_CAMERA_H__

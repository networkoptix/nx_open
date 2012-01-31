#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#include <core/dataconsumer/dataconsumer.h>
#include <core/resource/resource_consumer.h>

class QnAbstractMediaStreamDataProvider;
class QnVideoCameraGopKeeper;

class QnVideoCamera: public QObject
{
    Q_OBJECT
public:
    QnVideoCamera(QnResourcePtr resource);
    virtual ~QnVideoCamera();
    QnAbstractMediaStreamDataProvider* getLiveReader(QnResource::ConnectionRole role);
    void copyLastGop(bool primaryLiveStream, CLDataQueue& dstQueue);
    void beforeStop();
private slots:
    void onFpsChanged(QnAbstractMediaStreamDataProvider* provider, float value);
private:
    //QMutex m_queueMtx;
    QnResourcePtr m_resource;
    QnAbstractMediaStreamDataProvider* m_primaryReader;
    QnAbstractMediaStreamDataProvider* m_secondaryReader;

    QnVideoCameraGopKeeper* m_primaryGopKeeper;
    QnVideoCameraGopKeeper* m_secondaryGopKeeper;
};

#endif // __VIDEO_CAMERA_H__

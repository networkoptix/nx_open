#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#include "core/resource/resource.h"
#include "core/resource/resource_consumer.h"

class QnAbstractMediaStreamDataProvider;

class QnVideoCamera: public QnResourceConsumer, public QnAbstractDataConsumer
{
public:
    virtual void beforeDisconnectFromResource();
    QnVideoCamera(QnResourcePtr resource);
    virtual ~QnVideoCamera();
    QnAbstractMediaStreamDataProvider* getLiveReader();

    void copyLastGop(CLDataQueue& dstQueue);

    // QnAbstractDataConsumer
    virtual bool canAcceptData() const; 
    virtual void putData(QnAbstractDataPacketPtr data);
    virtual bool processData(QnAbstractDataPacketPtr data);
private:
    QnAbstractMediaStreamDataProvider* m_reader;
    QMutex m_queueMtx;
};

#endif // __VIDEO_CAMERA_H__

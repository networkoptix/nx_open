#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#include "core/resource/resource_consumer.h"

class QnAbstractMediaStreamDataProvider;

class QnVideoCamera: public QnResourceConsumer
{
public:
    virtual void beforeDisconnectFromResource();
    QnVideoCamera(QnResourcePtr resource);
    virtual ~QnVideoCamera();

    QnAbstractMediaStreamDataProvider* getLiveReader();
private:
    QnAbstractMediaStreamDataProvider* m_reader;
};

#endif // __VIDEO_CAMERA_H__

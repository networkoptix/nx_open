#ifndef __SERVER_STREAM_RECORDER_H__
#define __SERVER_STREAM_RECORDER_H__

#include "recording/stream_recorder.h"

class QnServerStreamRecorder: public QnStreamRecorder
{
public:
    QnServerStreamRecorder(QnResourcePtr dev);
protected:
    virtual bool processData(QnAbstractDataPacketPtr data);
private:
    bool processMotion(QnMetaDataV1Ptr motion);
};

#endif // __SERVER_STREAM_RECORDER_H__

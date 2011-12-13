#include "server_stream_recorder.h"
#include "motion/motion_helper.h"

QnServerStreamRecorder::QnServerStreamRecorder(QnResourcePtr dev):
    QnStreamRecorder(dev)
{

}

bool QnServerStreamRecorder::processMotion(QnMetaDataV1Ptr motion)
{
    QnMotionHelper::instance()->saveToArchive(motion);
    return true;
}

bool QnServerStreamRecorder::processData(QnAbstractDataPacketPtr data)
{
    QnMetaDataV1Ptr motion = qSharedPointerDynamicCast<QnMetaDataV1>(data);
    if (motion) 
        return processMotion(motion);
    else 
        return QnStreamRecorder::processData(data);
}

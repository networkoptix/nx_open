#ifndef __DUAL_STREAMING_HELPER_H__
#define __DUAL_STREAMING_HELPER_H__

#include <QMutex>
#include "core/datapacket/media_data_packet.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/resource_media_layout.h"
#include "qsharedpointer.h"

class QnDualStreamingHelper
{
public:
    QnDualStreamingHelper();
    virtual ~QnDualStreamingHelper();

    void updateCamera(QnSecurityCamResourcePtr cameraRes);
    qint64 getLastMotionTime();
    void onMotion(QnMetaDataV1Ptr motion);
private:
    QMutex m_mutex;
    qint64 m_lastMotionTime;
    __m128i *m_motionMaskBinData[CL_MAX_CHANNELS];
};

typedef QSharedPointer<QnDualStreamingHelper> QnDualStreamingHelperPtr;

#endif // __DUAL_STREAMING_HELPER_H__

#ifndef __DUAL_STREAMING_HELPER_H__
#define __DUAL_STREAMING_HELPER_H__

#include <utils/common/mutex.h>
#include "core/datapacket/media_data_packet.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/resource_media_layout.h"
#include "qsharedpointer.h"

class QnDualStreamingHelper
{
public:
    QnDualStreamingHelper();
    virtual ~QnDualStreamingHelper();

    qint64 getLastMotionTime();
    void onMotion(const QnMetaDataV1* motion);
private:
    QnMutex m_mutex;
    qint64 m_lastMotionTime;
};

typedef QSharedPointer<QnDualStreamingHelper> QnDualStreamingHelperPtr;

#endif // __DUAL_STREAMING_HELPER_H__

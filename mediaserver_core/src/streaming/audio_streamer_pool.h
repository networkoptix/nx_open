#ifndef AUDIO_STREAMER_POOL_H
#define AUDIO_STREAMER_POOL_H

#include <utils/common/singleton.h>
#include <core/resource/resource_fwd.h>
#include "camera/camera_pool.h"

class QnAudioStreamerPool : public Singleton<QnAudioStreamerPool>
{

public:
    QnAudioStreamerPool();

    bool startStreamToResource(const QString& clientId, const QString& resourceId, QString& error);
    bool stopStreamToResource(const QString& clientId, const QString& resourceId, QString& error);

private:
    QnVideoCameraPtr getTransmitSource(const QString& clientId)const;
    QnSecurityCamResourcePtr getTransmitDestination(const QString& resourceId) const;

};

#endif // AUDIO_STREAMER_POOL_H

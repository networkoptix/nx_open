#ifndef AUDIO_STREAMER_POOL_H
#define AUDIO_STREAMER_POOL_H

#include <utils/common/singleton.h>

class QnAudioStreamerPool : public Singleton<QnAudioStreamerPool>
{
public:
    QnAudioStreamerPool();

    bool startStreamToResource(const QString& clientId, const QString& resourceId);
    bool stopStreamToResource(const QString& clientId, const QString& resourceId);
};

#endif // AUDIO_STREAMER_POOL_H

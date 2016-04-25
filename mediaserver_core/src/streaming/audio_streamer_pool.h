#ifndef AUDIO_STREAMER_POOL_H
#define AUDIO_STREAMER_POOL_H

#include <utils/common/singleton.h>

class QnAudioStreamerPool : public Singleton<QnAudioStreamerPool>
{

public:
    QnAudioStreamerPool();

    enum class Action
    {
        Start,
        Stop
    };

    bool startStopStreamToResource(const QnUuid& clientId, const QnUuid& resourceId, Action action, QString& error);
};

#endif // AUDIO_STREAMER_POOL_H

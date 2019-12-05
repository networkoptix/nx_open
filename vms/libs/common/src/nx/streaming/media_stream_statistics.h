#pragma once

#include <nx/sdk/helpers/media_stream_statistics.h>

struct QnAbstractMediaData;
using QnAbstractMediaDataPtr = std::shared_ptr<QnAbstractMediaData>;

class QnMediaStreamStatistics: public nx::sdk::MediaStreamStatistics
{
public:
    QnMediaStreamStatistics() = default;

    void onData(const QnAbstractMediaDataPtr& media);
};

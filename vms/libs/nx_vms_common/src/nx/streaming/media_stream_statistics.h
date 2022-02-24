// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/helpers/media_stream_statistics.h>

#include <memory>

struct QnAbstractMediaData;
using QnAbstractMediaDataPtr = std::shared_ptr<QnAbstractMediaData>;

class NX_VMS_COMMON_API QnMediaStreamStatistics: public nx::sdk::MediaStreamStatistics
{
public:
    QnMediaStreamStatistics() = default;

    void onData(const QnAbstractMediaDataPtr& media);
};

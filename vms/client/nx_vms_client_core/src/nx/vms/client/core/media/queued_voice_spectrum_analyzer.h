// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <queue>
#include <utility>

#include <QtCore/QObject>

#include <nx/audio/format.h>
#include <nx/utils/thread/mutex.h>

#include "voice_spectrum_analyzer.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API QueuedVoiceSpectrumAnalyzer:
    public QObject
{
    Q_OBJECT

public:
    QueuedVoiceSpectrumAnalyzer();
    virtual ~QueuedVoiceSpectrumAnalyzer();

    void initialize(int srcSampleRate, int channels);

    void pushData(
        qint64 timestampUsec, const nx::audio::Format& format,
        const void* sampleData, int sampleBytes, qint64 maxQueueSizeUsec);

    SpectrumData readSpectrumData(qint64 timestampUsec);

    void reset();

private:
    std::unique_ptr<VoiceSpectrumAnalyzer> m_baseAnalyzer;
    std::queue<std::pair<qint64, SpectrumData>> m_queue; // (timestamp, data) pairs.
    mutable nx::Mutex m_mutex;
};

} // namespace nx::vms::client::core

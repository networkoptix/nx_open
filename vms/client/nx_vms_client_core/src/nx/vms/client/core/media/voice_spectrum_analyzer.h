// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QVector>

extern "C" {
#include <libavcodec/avfft.h> //< private
} // extern "C"

#include <nx/media/audio/format.h>
#include <nx/utils/thread/mutex.h>

namespace nx::vms::client::core {

struct SpectrumData
{
    QVector<double> data; //< data in range [0..1]
};

class NX_VMS_CLIENT_CORE_API VoiceSpectrumAnalyzer:
    public QObject
{
    Q_OBJECT

public:
    VoiceSpectrumAnalyzer();
    ~VoiceSpectrumAnalyzer();

    void initialize(int srcSampleRate, int channels);

    /**
     * Process input data. Data should be in 16 or 32 bit form. The total amount of samples is
     * sampleCount * channels.
     *
     * @returns                         Whether the stored spectrum data was updated.
     */
    bool processData(const qint16* sampleData, int sampleCount);
    bool processData(const qint32* sampleData, int sampleCount);
    bool processData(const nx::media::audio::Format& format, const void* sampleData, int sampleBytes);

    SpectrumData getSpectrumData() const;

    void reset();

    /** Number of bands in the result data. */
    static int bandsCount();

protected: //< Made protected for unit tests.
    template<class T>
    bool processDataInternal(const T* sampleData, int sampleCount);
    int windowSize() const { return m_windowSize; }
    FFTComplex* fftData() const { return m_fftData; }
    void performFft();
    static SpectrumData fillSpectrumData(const FFTComplex data[], int size, int srcSampleRate);

protected: //< Intended for experimenting and debugging, e.g. when changing the algorithm.
    static std::string asString(const double data[], int size);
    static std::string asString(const FFTComplex data[], int size);
    static std::string asString(const int16_t data[], int size);
    std::string dump() const;

private:
    int m_srcSampleRate = 0;
    int m_channels = 0;
    int m_windowSize = 0; /**< Number of complex values for FFT. */
    int m_bitCount = 0; /**< log2(m_windowSize) */
    FFTComplex* m_fftData = nullptr;
    FFTContext* m_fftContext = nullptr;
    int m_fftDataSize = 0; /**< Nmber of currently filled values in m_fftData. */
    SpectrumData m_spectrumData;
    mutable nx::Mutex m_mutex;
};

} // namespace nx::vms::client::core

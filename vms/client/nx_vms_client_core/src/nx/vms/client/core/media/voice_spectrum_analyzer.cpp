// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "voice_spectrum_analyzer.h"

#include <limits>

#include <QtCore/QtMath>

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>
#include <nx/media/media_fwd.h> //< for kMediaAlignment
#include <nx/utils/math/math.h>

namespace nx::vms::client::core {

static const int kUpdatesPerSecond = 30;
static const int kFreqStart = 50;
static const int kFreqEnd = 800;
static const int kFreqPerElement = 50;
static const int kBands = (kFreqEnd - kFreqStart) / kFreqPerElement;
static const double kBoostLevelDb = 2.5; //< max volume amplification for input data

/** Round value up to the power of 2. */
static quint32 toPowerOf2(quint32 v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

/**
 * Calculates the power of 2 not exceeding the given number.
 * @param value Must be a positive number.
 */
static int intLog2(int value)
{
    if (!NX_ASSERT(value > 0))
        return 0;

    int result = 0;
    int n = value;
    while (n >>= 1)
        ++result;

    return result;
}

void VoiceSpectrumAnalyzer::performFft()
{
    av_fft_permute(m_fftContext, m_fftData);
    av_fft_calc(m_fftContext, m_fftData);
}

VoiceSpectrumAnalyzer::VoiceSpectrumAnalyzer()
{
}

VoiceSpectrumAnalyzer::~VoiceSpectrumAnalyzer()
{
    nx::kit::utils::freeAligned(m_fftData);
    av_fft_end(m_fftContext);
}

void VoiceSpectrumAnalyzer::initialize(int srcSampleRate, int channels)
{
    // Check if already initialized.
    if (m_srcSampleRate == srcSampleRate && m_channels == channels)
        return;

    m_srcSampleRate = srcSampleRate;
    m_channels = channels;
    m_windowSize = toPowerOf2(srcSampleRate / kUpdatesPerSecond);
    m_bitCount = intLog2(m_windowSize);

    nx::kit::utils::freeAligned(m_fftData);
    m_fftData = static_cast<FFTComplex*>(nx::kit::utils::mallocAligned(
        sizeof(FFTComplex) * m_windowSize, nx::media::kMediaAlignment));
    memset(m_fftData, 0, m_windowSize * sizeof(m_fftData[0]));

    av_fft_end(m_fftContext);
    m_fftContext = av_fft_init(m_bitCount, /*inverse*/ 0);
}

bool VoiceSpectrumAnalyzer::processData(const qint16* sampleData, int sampleCount)
{
    return processDataInternal(sampleData, sampleCount);
}

bool VoiceSpectrumAnalyzer::processData(const qint32* sampleData, int sampleCount)
{
    return processDataInternal(sampleData, sampleCount);
}

bool VoiceSpectrumAnalyzer::processData(
    const nx::media::audio::Format& format,
    const void* sampleData,
    int sampleBytes)
{
    if (!NX_ASSERT(format.sampleType == nx::media::audio::Format::SampleType::signedInt) &&
        (format.sampleSize == 16 || format.sampleSize == 32))
    {
        return false;
    }

    if (format.sampleSize == 16)
    {
        return processData((const qint16*) sampleData, sampleBytes / 2);
    }
    else
    {
        return processData((const qint32*) sampleData, sampleBytes / 4);
    }
}

template<class T>
bool VoiceSpectrumAnalyzer::processDataInternal(const T* sampleData, int sampleCount)
{
    // Max volume amplification for input data.
    static const double kBoostLevel = 10 * log10(kBoostLevelDb);
    T maxAmplifier = std::numeric_limits<T>::max() / kBoostLevel;

    T maxSampleValue = 0;
    for (int i = 0; i < sampleCount * m_channels; ++i)
        maxSampleValue = qMax(maxSampleValue, T(abs(sampleData[i])));
    maxSampleValue = qMax(maxSampleValue, maxAmplifier);
    const T* p = sampleData;

    bool updated = false;
    for (int i = 0; i < sampleCount; ++i)
    {
        // Calculate an average for all channels. Channel samples are interleaved.
        qint64 sampleValue = 0;
        for (int channel = 0; channel < m_channels; ++channel)
            sampleValue += *(p++);
        sampleValue /= m_channels;

        m_fftData[m_fftDataSize].re = sampleValue / double(maxSampleValue);
        ++m_fftDataSize;

        if (m_fftDataSize == m_windowSize)
        {
            performFft();

            const SpectrumData spectrumData = fillSpectrumData(
                m_fftData, m_windowSize, m_srcSampleRate);
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                m_spectrumData = spectrumData;
            }

            updated = true;
            m_fftDataSize = 0;
            memset(m_fftData, 0, m_windowSize * sizeof(m_fftData[0]));
        }
    }
    return updated;
}

/*static*/ SpectrumData VoiceSpectrumAnalyzer::fillSpectrumData(
    const FFTComplex data[], int windowSize, int srcSampleRate)
{
    SpectrumData spectrumData;

    const double maxIndex = windowSize / 2;
    const double maxFreqSum = windowSize / 2;
    const double maxFreq = srcSampleRate / 2;
    const double freqPerElement = maxFreq / maxIndex;

    const int startIndex = kFreqStart / freqPerElement + 0.5;
    const int endIndex = kFreqEnd / freqPerElement + 0.5;
    const double stepsPerBand = (endIndex - startIndex + 1) / (double) kBands;

    double currentStep = 0.0;
    for (int currentIndex = startIndex; currentIndex <= endIndex;)
    {
        double value = 0.0;
        static const double epsilon = 1e-10;
        while (currentStep + epsilon < stepsPerBand)
        {
            const FFTComplex& complexNum = data[currentIndex];
            double fftMagnitude = sqrt(
                complexNum.re * complexNum.re + complexNum.im * complexNum.im);
            value += fftMagnitude;
            currentIndex++;
            currentStep += 1.0;
        }
        currentStep -= stepsPerBand;
        spectrumData.data.push_back(qBound(0.0, value / maxFreqSum, 1.0));
    }

    // Convert result data to Db.
    for (auto& data: spectrumData.data)
    {
        static const double kScaler = 9;
        data = log10(data * kScaler + 1.0);
    }

    return spectrumData;
}

SpectrumData VoiceSpectrumAnalyzer::getSpectrumData() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_spectrumData;
}

void VoiceSpectrumAnalyzer::reset()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_spectrumData = SpectrumData();
}

int VoiceSpectrumAnalyzer::bandsCount()
{
    return kBands;
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::string VoiceSpectrumAnalyzer::asString(const double data[], int size)
{
    std::string s;
    for (int i = 0; i < size; ++i)
    {
        s += nx::kit::utils::format("%f", data[i]);

        // Group 8 numbers in one line.
        if (i < size - 1)
        {
            if (i % 8 == 7)
                s += ",\n";
            else
                s += ", ";
        }
    }
    if (!s.empty())
        s += "\n";
    return s;
}

/*static*/ std::string VoiceSpectrumAnalyzer::asString(const FFTComplex data[], int size)
{
    std::string s;
    for (int i = 0; i < size; ++i)
    {
        s += nx::kit::utils::format("{%f, %f}", data[i].re, data[i].im);

        // Group 4 pairs in one line.
        if (i < size - 1)
        {
            if (i % 4 == 3)
                s += ",\n";
            else
                s += ", ";
        }
    }
    if (!s.empty())
        s += "\n";
    return s;
}

/*static*/ std::string VoiceSpectrumAnalyzer::asString(const int16_t data[], int size)
{
    std::string s;
    for (int i = 0; i < size; ++i)
    {
        s += nx::kit::utils::format("%d", data[i]);

        // Group 8 numbers in one line.
        if (i < size - 1)
        {
            if (i % 8 == 7)
                s += ",\n";
            else
                s += ", ";
        }
    }
    if (!s.empty())
        s += "\n";
    return s;
}

std::string VoiceSpectrumAnalyzer::dump() const
{
    std::string s;

    s += nx::kit::utils::format("m_srcSampleRate=%d\n", m_srcSampleRate);
    s += nx::kit::utils::format("m_channels=%d\n", m_channels);
    s += nx::kit::utils::format("m_windowSize=%d\n", m_windowSize);
    s += nx::kit::utils::format("m_bitCount=%d\n", m_bitCount);
    s += nx::kit::utils::format("m_fftDataSize=%d\n", m_fftDataSize);
    s += nx::kit::utils::format("m_fftContext=%s\n", m_fftContext ? "non-null" : "null");
    s += "m_spectrumData={\n";
    s += asString(m_spectrumData.data.data(), m_spectrumData.data.size());
    s += "}\n";
    s += "m_fftData={\n";
    s += asString(m_fftData, m_windowSize);
    s += "}\n";

    return s;
}

} // namespace nx::vms::client::core

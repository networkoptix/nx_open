// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "voice_spectrum_analyzer.h"

#include <QtCore/QtMath>

#include <nx/kit/utils.h>
#include <nx/kit/debug.h>
#include <nx/media/media_fwd.h> //< for kMediaAlignment
#include <utils/math/math.h>

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

void QnVoiceSpectrumAnalyzer::performFft()
{
    av_fft_permute(m_fftContext, m_fftData);
    av_fft_calc(m_fftContext, m_fftData);
}

QnSpectrumData::QnSpectrumData()
{
}

QnVoiceSpectrumAnalyzer::QnVoiceSpectrumAnalyzer()
{
}

QnVoiceSpectrumAnalyzer::~QnVoiceSpectrumAnalyzer()
{
    nx::kit::utils::freeAligned(m_fftData);
    av_fft_end(m_fftContext);
}

void QnVoiceSpectrumAnalyzer::initialize(int srcSampleRate, int channels)
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

void QnVoiceSpectrumAnalyzer::processData(const qint16* sampleData, int sampleCount)
{
    // Max volume amplification for input data.
    static const double kBoostLevel = 10 * log10(kBoostLevelDb);
    qint16 maxAmplifier = 32768 / kBoostLevel;

    qint16 maxSampleValue = 0;
    for (int i = 0; i < sampleCount * m_channels; ++i)
        maxSampleValue = qMax(maxSampleValue, qint16(abs(sampleData[i])));
    maxSampleValue = qMax(maxSampleValue, maxAmplifier);
    const qint16* p = sampleData;

    for (int i = 0; i < sampleCount; ++i)
    {
        // Calculate an average for all channels. Channel samples are interleaved.
        qint32 sampleValue = 0;
        for (int channel = 0; channel < m_channels; ++channel)
            sampleValue += *(p++);
        sampleValue /= m_channels;

        m_fftData[m_fftDataSize].re = sampleValue / double(maxSampleValue);
        ++m_fftDataSize;

        if (m_fftDataSize == m_windowSize)
        {
            performFft();

            const QnSpectrumData spectrumData = fillSpectrumData(
                m_fftData, m_windowSize, m_srcSampleRate);
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                m_spectrumData = spectrumData;
            }

            m_fftDataSize = 0;
            memset(m_fftData, 0, m_windowSize * sizeof(m_fftData[0]));
        }
    }
}

/*static*/ QnSpectrumData QnVoiceSpectrumAnalyzer::fillSpectrumData(
    const FFTComplex data[], int windowSize, int srcSampleRate)
{
    QnSpectrumData spectrumData;

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

QnSpectrumData QnVoiceSpectrumAnalyzer::getSpectrumData() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_spectrumData;
}

void QnVoiceSpectrumAnalyzer::reset()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_spectrumData = QnSpectrumData();
}

int QnVoiceSpectrumAnalyzer::bandsCount()
{
    return kBands;
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::string QnVoiceSpectrumAnalyzer::asString(const double data[], int size)
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

/*static*/ std::string QnVoiceSpectrumAnalyzer::asString(const FFTComplex data[], int size)
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

/*static*/ std::string QnVoiceSpectrumAnalyzer::asString(const int16_t data[], int size)
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

std::string QnVoiceSpectrumAnalyzer::dump() const
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

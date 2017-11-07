#include "voice_spectrum_analyzer.h"

#include <utils/math/math.h>

//#define USE_TEST_DATA

namespace
{

    static const int kUpdatesPerSecond = 30;
    static const int kFreqStart = 50;
    static const int kFreqEnd = 800;
    static const int kFreqPerElement = 50;
    static const int kBands = (kFreqEnd - kFreqStart) / kFreqPerElement;
    static const double kBoostLevelDb = 2.5; //< max volume amplification for input data

#ifdef USE_TEST_DATA
    void fillTestData(const qint16* sampleData, int sampleCount, int channels, int srcSampleRate)
    {
        static const double kToneFrequency = 1 * 1000;

        static double currentTime = 0.0;
        double tonePeriodInSamples = srcSampleRate / kToneFrequency;
        double stepPerSample = 2.0 * M_PI / tonePeriodInSamples;

        qint16* srcPtr = const_cast<qint16*>(sampleData);
        for (int n = 0; n < sampleCount; ++n)
        {
            //std::mt19937 gen;
            //std::uniform_int_distribution<qint64> dis(-32768, 32767);
            //double value = dis(gen);
            double value = sin(currentTime) * 32767;
            for (int ch = 0; ch < channels; ++ch)
                *srcPtr++ = value;
            currentTime += stepPerSample;
        }
    }
#endif


    /*
    FFT/IFFT routine. (see pages 507-508 of Numerical Recipes in C)

    Inputs:
    data[] : array of complex* data points of size 2*NFFT+1.
    data[0] is unused,
    * the n'th complex number x(n), for 0 <= n <= length(x)-1, is stored as:
    data[2*n+1] = real(x(n))
    data[2*n+2] = imag(x(n))
    if length(Nx) < NFFT, the remainder of the array must be padded with zeros

    nn : FFT order NFFT. This MUST be a power of 2 and >= length(x).
    isign:  if set to 1,
    computes the forward FFT
    if set to -1,
    computes Inverse FFT - in this case the output values have
    to be manually normalized by multiplying with 1/NFFT.
    Outputs:
    data[] : The FFT or IFFT results are stored in data, overwriting the input.
    */

    static const double TWOPI = 2.0 * M_PI;

    void fftTransform(double data[], int nn, int isign)
    {
        int n, mmax, m, j, istep, i;
        double wtemp, wr, wpr, wpi, wi, theta;
        double tempr, tempi;

        n = nn << 1;
        j = 1;
        for (i = 1; i < n; i += 2)
        {
            if (j > i)
            {
                tempr = data[j];     data[j] = data[i];     data[i] = tempr;
                tempr = data[j+1]; data[j+1] = data[i+1]; data[i+1] = tempr;
            }
            m = n >> 1;
            while (m >= 2 && j > m)
            {
                j -= m;
                m >>= 1;
            }
            j += m;
        }

        mmax = 2;
        while (n > mmax)
        {
            istep = 2*mmax;
            theta = TWOPI/(isign*mmax);
            wtemp = sin(0.5*theta);
            wpr = -2.0*wtemp*wtemp;
            wpi = sin(theta);
            wr = 1.0;
            wi = 0.0;
            for (m = 1; m < mmax; m += 2)
            {
                for (i = m; i <= n; i += istep)
                {
                    j = i + mmax;
                    tempr = wr*data[j]   - wi*data[j+1];
                    tempi = wr*data[j+1] + wi*data[j];
                    data[j]   = data[i]   - tempr;
                    data[j+1] = data[i+1] - tempi;
                    data[i] += tempr;
                    data[i+1] += tempi;
                }
                wr = (wtemp = wr)*wpr - wi*wpi + wr;
                wi = wi*wpr + wtemp*wpi + wi;
            }
            mmax = istep;
        }
    }

    /** Round value up to power of 2 */
    quint32 toPowerOf2(quint32 v)
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

} // unnamed namespace

QnSpectrumData::QnSpectrumData()
{

}

QnVoiceSpectrumAnalyzer::QnVoiceSpectrumAnalyzer() :
    m_srcSampleRate(0),
    m_windowSize(0),
    m_HannCoeff(),
    m_data(),
    m_dataSize(0),
    m_channels(0),
    m_spectrumData()
{}

void QnVoiceSpectrumAnalyzer::initialize(int srcSampleRate, int channels)
{
    /* Check if already initialized. */
    if (m_srcSampleRate == srcSampleRate && m_channels == channels)
        return;

    m_srcSampleRate = srcSampleRate;
    m_channels = channels;

    m_windowSize = toPowerOf2(srcSampleRate / kUpdatesPerSecond);
    m_HannCoeff.resize(m_windowSize);
    m_data.resize(m_windowSize * 2 + 1);

    // create Hann window filter
    for (int n = 0; n < m_windowSize; ++n)
        m_HannCoeff[n] = 0.5 * (1 - cos((2 * M_PI * n) / double(m_windowSize-1)));
}

void QnVoiceSpectrumAnalyzer::processData(const qint16* sampleData, int sampleCount)
{
#ifdef USE_TEST_DATA
    fillTestData(sampleData, sampleCount, m_channels, m_srcSampleRate);
#endif

    static const double kBoostLevel = 10 * log10(kBoostLevelDb); //< max volume amplification for input data
    qint16 maxAmplifier = 32768 / kBoostLevel;

    qint16 maxSampleValue = 0;
    for (int i = 0; i < sampleCount * m_channels; ++i)
        maxSampleValue = qMax(maxSampleValue, qint16(abs(sampleData[i])));
    maxSampleValue = qMax(maxSampleValue, maxAmplifier);

    const qint16* curPtr = sampleData;
    for (int i = 0; i < sampleCount; ++i)
    {
        qint32 sampleValue = 0;
        for (int ch = 0; ch < m_channels; ++ch)
            sampleValue += *curPtr++;
        sampleValue /= m_channels;

        int index = m_dataSize * 2 + 1;
        m_data[index] = (sampleValue / double(maxSampleValue)) * m_HannCoeff[m_dataSize]; // << Hann filtering
        if (++m_dataSize == m_windowSize)
        {
            fftTransform(m_data.data(), m_windowSize, 1);
            fillSpectrumData();
            m_dataSize = 0;
            std::fill(m_data.begin(), m_data.end(), 0.0);
        }
    }
}

void QnVoiceSpectrumAnalyzer::fillSpectrumData()
{
    QnSpectrumData result;

    const double maxIndex = m_windowSize / 2;
    const double maxFreqSum = m_windowSize / 2;
    const double maxFreq = m_srcSampleRate / 2;
    const double freqPerElement = maxFreq / maxIndex;

    const int startIndex = kFreqStart / freqPerElement + 0.5;
    const int endIndex = kFreqEnd / freqPerElement + 0.5;
    const double stepsPerBand = (endIndex - startIndex + 1) / (double) kBands;

    double currentStep = 0.0;
    for(int currentIndex = startIndex; currentIndex <= endIndex;)
    {
        double value = 0.0;
        static const double epsilon = 1e-10;
        while (currentStep + epsilon < stepsPerBand)
        {
            double* complexNum = &m_data[currentIndex * 2 + 1];
            double fftMagnitude = sqrt(complexNum[0] * complexNum[0] + complexNum[1] * complexNum[1]);
            value += fftMagnitude;
            currentIndex++;
            currentStep += 1.0;
        }
        currentStep -= stepsPerBand;
        result.data.push_back(qBound(0.0, value / maxFreqSum, 1.0));
    }

    // convert result data to Db
    for (auto& data: result.data)
    {
        static const double kScaler = 9;
        data = log10(data * kScaler + 1.0);
    }

    QnMutexLocker lock(&m_mutex);
    m_spectrumData = result;
}

QnSpectrumData QnVoiceSpectrumAnalyzer::getSpectrumData() const
{
    QnMutexLocker lock(&m_mutex);
    return m_spectrumData;
}

void QnVoiceSpectrumAnalyzer::reset()
{
    QnMutexLocker lock(&m_mutex);
    m_spectrumData = QnSpectrumData();
}

int QnVoiceSpectrumAnalyzer::bandsCount()
{
    return kBands;
}

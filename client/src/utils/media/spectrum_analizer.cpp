#include "spectrum_analizer.h"
#include "math.h"

namespace {

    static const int kUpdatesPerSecond = 4;
    static const int kBands = 16;

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

static const float TWOPI = 2.0 * M_PI;

void four1(float data[], int nn, int isign)
{
    int n, mmax, m, j, istep, i;
    float wtemp, wr, wpr, wpi, wi, theta;
    float tempr, tempi;
    
    n = nn << 1;
    j = 1;
    for (i = 1; i < n; i += 2) {
	if (j > i) {
	    tempr = data[j];     data[j] = data[i];     data[i] = tempr;
	    tempr = data[j+1]; data[j+1] = data[i+1]; data[i+1] = tempr;
	}
	m = n >> 1;
	while (m >= 2 && j > m) {
	    j -= m;
	    m >>= 1;
	}
	j += m;
    }
    mmax = 2;
    while (n > mmax) {
	istep = 2*mmax;
	theta = TWOPI/(isign*mmax);
	wtemp = sin(0.5*theta);
	wpr = -2.0*wtemp*wtemp;
	wpi = sin(theta);
	wr = 1.0;
	wi = 0.0;
	for (m = 1; m < mmax; m += 2) {
	    for (i = m; i <= n; i += istep) {
		j =i + mmax;
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

QnSpectrumAnalizer::QnSpectrumAnalizer(int srcSampleRate, int channels):
        m_dataSize(0),
        m_channels(channels)
{
    m_windowSize = toPowerOf2(srcSampleRate / kUpdatesPerSecond);
    m_HannCoeff.resize(m_windowSize);
    m_data.resize(m_windowSize * 2 + 1);
    m_fftMagnitude.resize(m_windowSize / 2);
    
    // create Hann window filter
    for (int n = 0; n < m_windowSize; ++n)
    {
        m_HannCoeff[n] = 0.5 * (1 - cos((2 * M_PI * n) / float(m_windowSize-1)));
    }
}

void QnSpectrumAnalizer::processData(const qint16* sampleData, int sampleCount)
{
    const qint16* curPtr = sampleData;
    for (int i = 0; i < sampleCount; ++i)
    {
        qint32 sampleValue = 0;
        for (int ch = 0; ch < m_channels; ++ch)
            sampleValue += *curPtr++;
        sampleValue /= m_channels;

        int index = m_dataSize * 2 + 1;
        m_data[index] = (sampleValue / 32768.0) * m_HannCoeff[m_dataSize]; // << Hann filtering
        if (++m_dataSize == m_windowSize)
        {
            four1(m_data.data(), m_windowSize, 1); //< FFT transform
            for (int n = 0; n < m_windowSize / 2; ++n)
            {
                float* complexNum = &m_data[n * 2 + 1];
                m_fftMagnitude[n] = sqrt(complexNum[0] * complexNum[0] + complexNum[1] * complexNum[1]);
                m_fftMagnitude[n] = 10 * log10(m_fftMagnitude[n]); //< to db
            }
            m_dataSize = 0;
        }
        m_spectrumData = fillSpectrumData();
    }
}

QnSpectrumData QnSpectrumAnalizer::fillSpectrumData() const
{
    QnSpectrumData result;
    
    int stepsPerBand = (m_windowSize / 2) / kBands;
    const float* srcData = &m_fftMagnitude[0];
    for (int i = 0; i < kBands; ++i)
    {
        float value = 0.0;
        for (int j = 0; j < stepsPerBand; ++j)
            value += *srcData++;
        result.data.push_back(value);
    }

    return result;
}

QnSpectrumData QnSpectrumAnalizer::getSpectrumData() const
{
    return m_spectrumData;
}

#pragma once

#include <utils/common/singleton.h>

struct QnSpectrumData
{
    QnSpectrumData();

    QVector<float> data; //< data in range [0..1]
};

class QnSpectrumAnalizer: public QObject, public Singleton<QnSpectrumAnalizer>
{
    Q_OBJECT
public:
    QnSpectrumAnalizer();
    void initialize(int srcSampleRate, int channels);

    /**
     * Process input data. Data should be in 16 bit form.
     * Total amount of samples is sampleCount * channels
    */
    void processData(const qint16* sampleData, int sampleCount);

    QnSpectrumData getSpectrumData() const;
private:
    QnSpectrumData fillSpectrumData() const;
private:
    int m_srcSampleRate;
    int m_windowSize;
    QVector<float> m_HannCoeff;
    QVector<float> m_data;
    QVector<float> m_fftMagnitude;
    int m_dataSize;
    int m_channels;
    QnSpectrumData m_spectrumData;
};

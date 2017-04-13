#pragma once

#include <nx/utils/singleton.h>

struct QnSpectrumData
{
    QnSpectrumData();

    QVector<double> data; //< data in range [0..1]
};

class QnVoiceSpectrumAnalyzer: public QObject, public Singleton<QnVoiceSpectrumAnalyzer>
{
    Q_OBJECT
public:
    QnVoiceSpectrumAnalyzer();
    void initialize(int srcSampleRate, int channels);

    /**
     * Process input data. Data should be in 16 bit form.
     * Total amount of samples is sampleCount * channels
    */
    void processData(const qint16* sampleData, int sampleCount);

    QnSpectrumData getSpectrumData() const;

    void reset();

    /** Number of bands in the result data. */
    static int bandsCount();
private:
    void fillSpectrumData();
private:
    int m_srcSampleRate;
    int m_windowSize;
    QVector<double> m_HannCoeff;
    QVector<double> m_data;
    int m_dataSize;
    int m_channels;
    QnSpectrumData m_spectrumData;
    mutable QnMutex m_mutex;
};

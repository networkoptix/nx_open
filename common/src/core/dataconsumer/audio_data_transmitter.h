#pragma once

#include <core/dataconsumer/abstract_data_consumer.h>

struct QnOutputAudioFormat
{
    const static int kDefaultSampleRate = -1;
    QnOutputAudioFormat(CodecID codec, int sampleRate):
        codec(codec),
        sampleRate(sampleRate)
    {
    }

    QnOutputAudioFormat():
        codec(CODEC_ID_NONE),
        sampleRate(kDefaultSampleRate)
    {
    }

    bool isEmpty() const { return codec == CODEC_ID_NONE; }

    CodecID codec;
    int sampleRate;
};


class QnAbstractAudioTransmitter : public QnAbstractDataConsumer
{
    Q_OBJECT
public:

    QnAbstractAudioTransmitter();

    virtual bool processData(const QnAbstractDataPacketPtr &data) override;
    virtual bool processAudioData(QnConstAbstractMediaDataPtr &data) = 0;

    /**
     * A bit hack here: if open desktop camera without configuration then only audio stream is opened.
     * Otherwise (default) video + audio is opened
     */
    virtual bool needConfigureProvider() const { return false; }

    /**
     * Returns true if transmitter is compatible with AudioFormat
     * Otherwise (default) video + audio is opened
     */
    virtual bool isCompatible(const QnOutputAudioFormat& format) const { return false; }

    /**
     * Set output format for transmitter
     */
    virtual void setOutputFormat(const QnOutputAudioFormat& format) = 0;

    /**
     * Returns true if transmitter is ready to use
     */
    virtual bool isInitialized() const = 0;
private:
    QnMutex m_mutex;
};

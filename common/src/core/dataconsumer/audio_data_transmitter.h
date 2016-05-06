#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <core/dataconsumer/abstract_data_consumer.h>
#include <core/dataprovider/live_stream_provider.h>
#include <core/datapacket/abstract_data_packet.h>
#include <core/datapacket/media_data_packet.h>

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
    virtual bool isCompatible(const QnAudioFormat& format) const { return false; }

    /**
     * Set output format for transmitter
     */
    virtual void setOutputFormat(const QnAudioFormat& format) = 0;

    /**
     * Returns true if transmitter is ready to use
     */
    virtual bool isInitialized() const = 0;

    virtual void prepare() {}


    virtual void unsubscribe(QnAbstractStreamDataProviderPtr desktopDataProvider);
    virtual void subscribe(QnAbstractStreamDataProviderPtr desktopDataProvider);

private:
    QnMutex m_mutex;
    QnAbstractStreamDataProviderPtr m_dataProvider;
};

#endif // #ifdef ENABLE_DATA_PROVIDERS

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

    static const int kContinuousNotificationPriority = 10;
    static const int kUserPriority = 20;
    static const int kSingleNotificationPriority = 30;

    QnAbstractAudioTransmitter();

    virtual bool processData(const QnAbstractDataPacketPtr &data) override;
    virtual bool processAudioData(const QnConstCompressedAudioDataPtr& data) = 0;

    /**
     * A bit hack here: if open desktop camera without configuration then only audio stream is opened.
     * Otherwise (default) video + audio is opened
     */
    virtual bool needConfigureProvider() const { return false; }

    /**
     * Returns true if transmitter is compatible with AudioFormat
     * Otherwise (default) video + audio is opened
     */
    virtual bool isCompatible(const QnAudioFormat& /*format*/) const { return false; }

    /**
     * Set output format for transmitter
     */
    virtual void setOutputFormat(const QnAudioFormat& format) = 0;

    /**
     * Returns true if transmitter is ready to use
     */
    virtual bool isInitialized() const = 0;

    virtual void prepare() {}

    virtual void subscribe(
        QnAbstractStreamDataProviderPtr desktopDataProvider,
        int priorityClass = kUserPriority);

    virtual void unsubscribe(QnAbstractStreamDataProvider* dataProvider);

protected:
    virtual void endOfRun() override;
private:
    virtual void makeRealTimeDelay(const QnConstCompressedAudioDataPtr& audioData);
    void unsubscribeUnsafe(QnAbstractStreamDataProvider* dataProvider);
    void removePacketsByProvider(QnAbstractStreamDataProvider* dataProvider);
private:
    QnMutex m_mutex;
    std::map<qint64, QnAbstractStreamDataProviderPtr, std::greater<qint64>> m_providers;
    QElapsedTimer m_elapsedTimer;
    quint64 m_transmittedPacketDuration;
    QnAbstractStreamDataProvider* m_prevUsedProvider;
};

#endif // #ifdef ENABLE_DATA_PROVIDERS

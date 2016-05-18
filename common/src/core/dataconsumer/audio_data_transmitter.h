#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <core/dataconsumer/abstract_data_consumer.h>
#include <core/dataprovider/live_stream_provider.h>
#include <core/datapacket/abstract_data_packet.h>
#include <core/datapacket/media_data_packet.h>

struct QnDataProviderInfo
{
    QnDataProviderInfo(
        const QnAbstractStreamDataProviderPtr& providerPtr,
        int priorityValue = kContinuousNotificationPriority) :
        provider(providerPtr),
        timestamp(QDateTime::currentMSecsSinceEpoch()),
        priority(priorityValue){}

    QnAbstractStreamDataProviderPtr provider;
    quint64 timestamp;
    int priority;

    bool operator <(const QnDataProviderInfo& other)
    {
        if(this->priority != other.priority)
            return this->priority < other.priority;
        else
            return this->timestamp < other.timestamp;
    }

    bool operator ==(const QnDataProviderInfo& other)
    {
        return this->provider == other.provider;
    }

    static const int kUserPriority = 20;
    static const int kSingleNotificationPriority = 30;
    static const int kContinuousNotificationPriority = 10;
};

typedef std::shared_ptr<QnDataProviderInfo> QnDataProviderInfoPtr;

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

    virtual void subscribe(
        QnAbstractStreamDataProviderPtr desktopDataProvider,
        int priority = QnDataProviderInfo::kUserPriority);

    virtual void unsubscribe(QnAbstractStreamDataProviderPtr desktopDataProvider);

protected:
    void run();
    virtual void makeRealTimeDelay();

private:
    void enqueueProvider(const QnDataProviderInfo& info);
    void dequeueProvider(const QnDataProviderInfo& info);

private:
    QnMutex m_mutex;
    QnDataProviderInfo m_dataProviderInfo;
    QList<QnDataProviderInfo> m_waitingProviders;
    QElapsedTimer m_elapsedTimer;
    quint64 m_transmittedPacketCount;
    quint64 m_transmittedPacketDuration;

};

#endif // #ifdef ENABLE_DATA_PROVIDERS

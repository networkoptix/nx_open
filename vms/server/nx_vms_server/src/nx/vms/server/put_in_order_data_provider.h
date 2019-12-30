#pragma once

#include <deque>
#include <chrono>

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/data_structures/top_queue2.h>

namespace nx::vms::server {

    /*
     * This class allow to reorder input media by their timestamps.
     * Required reordering buffer size is calculated automatically.
     */
    class AbstractDataReorderer
    {
    public:

        constexpr static const std::chrono::milliseconds kMinQueueDuration =
            std::chrono::milliseconds(0);
        constexpr static const std::chrono::seconds kMaxQueueDuration =
            std::chrono::seconds(2);

        enum class BufferingPolicy
        {
            increaseAndDescrease,
            increaseOnly
        };

        struct Settings
        {
            std::chrono::microseconds minQueueDuration = kMinQueueDuration;
            std::chrono::microseconds maxQueueDuration = kMaxQueueDuration;
            std::chrono::microseconds initialQueueDuration = kMinQueueDuration;
            BufferingPolicy policy = BufferingPolicy::increaseAndDescrease;
        };

        /*
         * This class allow to reorder input media by their timestamps.
         * @param provider source data provider.
         * @param initialQueueDuration initial reordering queue length.
         * @param minQueueDuration minimum reordering queue length.
         * @param maxQueueDuration maximum reordering queue length.
         * @param BufferingPolicy auto increase/decrease buffer or increase only.
         */
        AbstractDataReorderer(const Settings& settings);

        virtual ~AbstractDataReorderer() = default;

        void processNewData(const QnAbstractDataPacketPtr& data);

        /*
         * Flush Unprocessed (buffered) data. For each media packet
         * virtual function 'flushData' is called.
         */
        std::optional<std::chrono::microseconds> flush();
    protected:
        virtual void flushData(const QnAbstractDataPacketPtr& data) = 0;
    private:
        void updateBufferSize(const QnAbstractDataPacketPtr& data);
    private:
        struct JitterInfo
        {
            std::chrono::microseconds timestamp{};
            std::chrono::microseconds jitter{};

            bool operator<(const JitterInfo& right) const { return jitter < right.jitter; }
        };

        constexpr static const std::chrono::seconds kJitterBufferSize =
            std::chrono::minutes(2);

        std::deque<QnAbstractMediaDataPtr> m_dataQueue;
        std::chrono::microseconds m_queueDuration;

        nx::utils::ElapsedTimer m_timer;

        const Settings m_settings;
        nx::utils::QueueWithMax<JitterInfo> m_lastJitter;
        QnMutex m_mutex;
    };

    class ProxyDataProvider: public QnAbstractStreamDataProvider
    {
    public:
        ProxyDataProvider(const QnAbstractStreamDataProviderPtr& provider);

        virtual bool dataCanBeAccepted() const override;
        virtual bool isReverseMode() const override;
        virtual void disconnectFromResource() override;
        virtual void setRole(Qn::ConnectionRole role) override;
        virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;
        virtual bool hasVideo() const override;
        virtual bool needConfigureProvider() const override;
        virtual void startIfNotRunning() override;
        virtual QnSharedResourcePointer<QnAbstractVideoCamera> getOwner() const override;
    protected:
        QnAbstractStreamDataProviderPtr m_provider = nullptr;
    };

    class PutInOrderDataProvider:
        public ProxyDataProvider,
        public AbstractDataReorderer,
        public QnAbstractMediaDataReceptor
    {
    public:
        PutInOrderDataProvider(
            const QnAbstractStreamDataProviderPtr& provider, const Settings& settings);

        virtual ~PutInOrderDataProvider();

        virtual bool canAcceptData() const override { return true; }
        virtual void stop() override;
        virtual void flushData(const QnAbstractDataPacketPtr& data) override;
        virtual void putData(const QnAbstractDataPacketPtr& data) override;
    protected:
        virtual void start(Priority priority = InheritPriority) override;
    };

    class SimpleReorderer: public AbstractDataReorderer
    {
    public:
        SimpleReorderer(): AbstractDataReorderer(Settings()) {}

        virtual void flushData(const QnAbstractDataPacketPtr& data) override;
        std::deque<QnAbstractDataPacketPtr>& queue();
    private:
        std::deque<QnAbstractDataPacketPtr> m_queue;
    };


    using PutInOrderDataProviderPtr = QSharedPointer<PutInOrderDataProvider>;

} // namespace nx::vms::server

#pragma once

#include <deque>
#include <chrono>

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/data_structures/top_queue2.h>

namespace nx::vms::server {

    class ProxyDataProvider:
        public QnAbstractStreamDataProvider,
        public QnAbstractMediaDataReceptor
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
        virtual void start(Priority priority = InheritPriority) override;
        virtual void stop() override;
    protected:
        QnAbstractStreamDataProviderPtr m_provider = nullptr;
    };

    /*
     * This class allow to reorder input media by their timestamps.
     * It works as a proxy provider between consumer and source data provider.
     * Required reordering buffer size is calculated automatically.
     */
    class PutInOrderDataProvider: public ProxyDataProvider
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

        /*
         * This class allow to reorder input media by their timestamps.
         * @param provider source data provider.
         * @param initialQueueDuration initial reordering queue length.
         * @param minQueueDuration minimum reordering queue length.
         * @param maxQueueDuration maximum reordering queue length.
         * @param BufferingPolicy auto increase/decrease buffer or increase only.
         */
        PutInOrderDataProvider(
            const QnAbstractStreamDataProviderPtr& provider,
            std::chrono::microseconds initialQueueDuration = kMinQueueDuration,
            std::chrono::microseconds minQueueDuration = kMinQueueDuration,
            std::chrono::microseconds maxQueueDuration = kMaxQueueDuration,
            BufferingPolicy policy = BufferingPolicy::increaseAndDescrease);
        virtual ~PutInOrderDataProvider();

        virtual void putData(const QnAbstractDataPacketPtr& data) override;
        virtual bool canAcceptData() const override { return true; }
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

        std::chrono::microseconds m_minQueueDuration{};
        std::chrono::microseconds m_maxQueueDuration{};
        nx::utils::QueueWithMax<JitterInfo> m_lastJitter;
        BufferingPolicy m_policy = BufferingPolicy::increaseAndDescrease;
        qint64 m_lastOutputTime = 0;
    };

    using PutInOrderDataProviderPtr = QSharedPointer<PutInOrderDataProvider>;

} // namespace nx::vms::server

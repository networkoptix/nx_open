#pragma once

#include <set>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/server/analytics/stream_data_receptor.h>

namespace nx::vms::server::analytics {

class ProxyStreamDataReceptor: public StreamDataReceptor
{
public:
    ProxyStreamDataReceptor() = default;
    ProxyStreamDataReceptor(QWeakPointer<StreamDataReceptor> receptor);

    void setProxiedReceptor(QWeakPointer<StreamDataReceptor> receptor);

    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    virtual StreamProviderRequirements providerRequirements(
        nx::vms::api::StreamIndex streamIndex) const override;

    virtual void registerStream(nx::vms::api::StreamIndex streamIndex) override;

private:
    void registerStreamsToProxiedReceptorThreadUnsafe();

private:
    mutable QnMutex m_mutex;
    QWeakPointer<StreamDataReceptor> m_proxiedReceptor;
    std::set<nx::vms::api::StreamIndex> m_registeredStreamIndexes;
};

using ProxyStreamDataReceptorPtr = QSharedPointer<ProxyStreamDataReceptor>;

} // namespace nx::vms::server::analytics

#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/vms/server/analytics/i_stream_data_receptor.h>

namespace nx::vms::server::analytics {

class ProxyStreamDataReceptor: public IStreamDataReceptor
{
public:
    ProxyStreamDataReceptor() = default;
    ProxyStreamDataReceptor(QWeakPointer<IStreamDataReceptor> receptor);

    void setProxiedReceptor(QWeakPointer<IStreamDataReceptor> receptor);

    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    virtual nx::vms::api::analytics::StreamTypes requiredStreamTypes() const override;

private:
    mutable QnMutex m_mutex;
    QWeakPointer<IStreamDataReceptor> m_proxiedReceptor;
};

using ProxyStreamDataReceptorPtr = QSharedPointer<ProxyStreamDataReceptor>;

} // namespace nx::vms::server::analytics

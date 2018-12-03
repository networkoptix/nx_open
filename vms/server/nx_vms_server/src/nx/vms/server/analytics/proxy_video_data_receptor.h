#pragma once

#include <nx/vms/server/analytics/abstract_video_data_receptor.h>

namespace nx::vms::server::analytics {

class ProxyVideoDataReceptor: public AbstractVideoDataReceptor
{
public:
    ProxyVideoDataReceptor() = default;
    ProxyVideoDataReceptor(QWeakPointer<AbstractVideoDataReceptor> receptor);

    void setProxiedReceptor(QWeakPointer<AbstractVideoDataReceptor> receptor);

    virtual bool needsCompressedFrames() const override;
    virtual NeededUncompressedPixelFormats neededUncompressedPixelFormats() const override;

    virtual void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame) override;

private:
    mutable QnMutex m_mutex;
    QWeakPointer<AbstractVideoDataReceptor> m_proxiedReceptor;
};

using ProxyVideoDataReceptorPtr = QSharedPointer<ProxyVideoDataReceptor>;

} // namespace nx::vms::server::analytics

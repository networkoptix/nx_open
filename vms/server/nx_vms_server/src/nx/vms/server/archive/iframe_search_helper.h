#pragma once

#include <nx/vms/server/archive/abstract_iframe_search_helper.h>

class QnResourcePool;
class QnVideoCameraPool;

namespace nx::vms::server::archive {

class IframeSearchHelper:
    public QObject,
    public AbstractIframeSearchHelper
{
    Q_OBJECT
public:
    IframeSearchHelper(
        const QnResourcePool* resourcePool,
        const QnVideoCameraPool* cameraPool);

    virtual qint64 findAfter(
        const QnUuid& deviceId,
        nx::vms::api::StreamIndex streamIndex,
        qint64 timestampUs) const override;
private:
    const QnResourcePool* m_resourcePool = nullptr;
    const QnVideoCameraPool* m_cameraPool = nullptr;
};

} // namespace nx::vms::server::archive

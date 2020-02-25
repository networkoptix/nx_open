#pragma once

#include "abstract_i_frame_search_helper.h"

class QnResourcePool;
class QnVideoCameraPool;

namespace nx::vms::server::analytics {

class IFrameSearchHelper: public QObject, public AbstractIFrameSearchHelper
{
    Q_OBJECT
public:
    IFrameSearchHelper(
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

} // namespace nx::vms::server::analytics

#pragma once

#include "../abstract_async_search_list_model.h"

#include <limits>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

class QTimer;
class QnUuid;

namespace nx {
namespace client {
namespace desktop {

class AbstractAsyncSearchListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(AbstractAsyncSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    virtual void setCamera(const QnVirtualCameraResourcePtr& camera);

    virtual int count() const = 0;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const = 0;

    virtual void clear();

    bool canFetchMore() const;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commit(qint64 earliestTimeToCommitMs);
    bool fetchInProgress() const;

protected:
    virtual rest::Handle requestPrefetch(qint64 latestTimeMs) = 0;
    virtual bool commitPrefetch(qint64 earliestTimeToCommitMs, bool& fetchedAll) = 0;
    virtual bool hasAccessRights() const = 0;

    bool shouldSkipResponse(rest::Handle requestId) const;
    void complete(qint64 earliestTimeMs); //< Calls and clears m_prefetchCompletionHandler.

    qint64 earliestTimeMs() const;

private:
    AbstractAsyncSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    qint64 m_earliestTimeMs = std::numeric_limits<qint64>::max();
    rest::Handle m_currentFetchId = rest::Handle();
    PrefetchCompletionHandler m_prefetchCompletionHandler;
    bool m_fetchedAll = false;
};

} // namespace
} // namespace client
} // namespace nx

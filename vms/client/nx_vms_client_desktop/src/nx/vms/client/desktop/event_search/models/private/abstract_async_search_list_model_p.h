#pragma once

#include "../abstract_async_search_list_model.h"

#include <limits>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class AbstractAsyncSearchListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(AbstractAsyncSearchListModel* q);
    virtual ~Private() override;

    virtual int count() const = 0;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const = 0;

    virtual void clearData() = 0;
    virtual void truncateToMaximumCount() = 0;
    virtual void truncateToRelevantTimePeriod() = 0;

    bool requestFetch();
    bool fetchInProgress() const;
    void cancelPrefetch();

    using PrefetchCompletionHandler = std::function<
        void(const QnTimePeriod& fetchedPeriod, FetchResult result)>;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commit(const QnTimePeriod& periodToCommit);

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) = 0;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) = 0;

    void completePrefetch(const QnTimePeriod& actuallyFetched, bool success, int fetchedCount);

    struct FetchInformation
    {
        rest::Handle id = rest::Handle();
        FetchDirection direction = FetchDirection::earlier;
        QnTimePeriod period;
        int batchSize = 0;
    };

    const FetchInformation& currentRequest() const;

private:
    AbstractAsyncSearchListModel * const q;
    PrefetchCompletionHandler m_prefetchCompletionHandler;
    FetchInformation m_request;
};

} // namespace nx::vms::client::desktop

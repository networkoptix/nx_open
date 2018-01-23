#pragma once

#include <functional>

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/client/desktop/event_search/models/abstract_event_list_model.h>

namespace nx {
namespace client {
namespace desktop {

class AbstractAsyncSearchListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

protected:
    class Private;
    using CreatePrivate = std::function<Private*()>;
    explicit AbstractAsyncSearchListModel(CreatePrivate dCreator, QObject* parent = nullptr);
    Private* d_func();

public:
    virtual ~AbstractAsyncSearchListModel() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    QnTimePeriod selectedTimePeriod() const;
    void setSelectedTimePeriod(const QnTimePeriod& value);

    void clear();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;

    // An alternative to fetchMore that does not insert new rows immediately but calls
    //   completionHandler instead. To finalize fetch commitPrefetch must be called.
    //   Must return false and do nothing if previous fetch is not committed yet.
    //   Time range synchronization between several models is possible with
    //   the help of earliestTimeFetchedMs/earliestTimeToCommitMs arguments.
    //   If prefetch is cancelled, PrefetchCompletionHandler is called with -1.
    using PrefetchCompletionHandler = std::function<void(qint64 earliestTimeFetchedMs)>;
    virtual bool prefetchAsync(PrefetchCompletionHandler completionHandler);
    virtual void commitPrefetch(qint64 earliestTimeToCommitMs);
    virtual bool fetchInProgress() const;

private:
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

#pragma once

#include "../motion_search_list_model.h"

#include <deque>
#include <bitset>
#include <vector>

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QTimer>

#include <motion/motion_detection.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <nx/vms/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>

class QMenu;
class QTimer;

namespace nx::vms::client::desktop {

struct MotionChunk
{
    QnUuid cameraId;
    QnTimePeriod period;

    bool operator==(const MotionChunk& other) const
    {
        return cameraId == other.cameraId && period == other.period;
    }
};

class MotionSearchListModel::Private: public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

public:
    explicit Private(MotionSearchListModel* q);
    virtual ~Private() override;

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    QList<QRegion> filterRegions() const;
    void setFilterRegions(const QList<QRegion>& value);

    virtual void clearData() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;

    bool isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const;

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) override;

private:
    template<typename Iter>
    bool commitInternal(const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd,
        int position, bool handleOverlaps);

    rest::Handle getMotion(const QnTimePeriod& period, Qt::SortOrder order, int limit);

    Q_SLOT void processReceivedTimePeriods(
        int status, const MultiServerPeriodDataList& timePeriods, int requestId);

    void fetchLive();

    QSharedPointer<QMenu> contextMenu(const MotionChunk& chunk) const;
    QnVirtualCameraResourcePtr camera(const MotionChunk& chunk) const;

private:
    MotionSearchListModel* const q;
    QList<QRegion> m_filterRegions;

    std::vector<MotionChunk> m_prefetch;
    std::deque<MotionChunk> m_data;

    QScopedPointer<QTimer> m_liveUpdateTimer;
    FetchInformation m_liveFetch;
};

} // namespace nx::vms::client::desktop

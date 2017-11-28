#pragma once

#include "../motion_search_list_model.h"

#include <deque>

#include <camera/camera_data_manager.h>
#include <recording/time_period_list.h>

namespace nx {
namespace client {
namespace desktop {

class MotionSearchListModel::Private: public QObject
{
    Q_OBJECT

public:
    explicit Private(MotionSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    int count() const;
    const QnTimePeriod& period(int index) const;

    bool canFetchMore() const;
    void fetchMore();

private:
    void updateMotionPeriods(qint64 startTimeMs);
    void reset();

private:
    MotionSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    QnCachingCameraDataLoaderPtr m_loader;
    std::deque<QnTimePeriod> m_data; //< Reversed list.
};

} // namespace
} // namespace client
} // namespace nx

#pragma once

#include "../motion_search_list_model.h"

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

    bool canFetchMore() const;
    void fetchMore();

private:
    void periodsChanged(qint64 startTimeMs);
    void addPeriod(const QnTimePeriod& period, Position where);
    qint64 firstTimeMs() const;
    qint64 lastTimeMs() const;

private:
    MotionSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    QnCachingCameraDataLoaderPtr m_loader;
};

} // namespace
} // namespace client
} // namespace nx

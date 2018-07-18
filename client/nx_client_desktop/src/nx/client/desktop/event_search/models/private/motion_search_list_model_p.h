#pragma once

#include "../motion_search_list_model.h"

#include <deque>

#include <QtCore/QSharedPointer>
#include <QtWidgets/QMenu>

#include <camera/camera_data_manager.h>

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

    QSharedPointer<QMenu> contextMenu(int index) const;

    bool canFetchMore() const;
    void fetchMore();

    void reset();

    int totalCount() const;

private:
    void updateMotionPeriods(qint64 startTimeMs);
    QnTimePeriodList periods() const;

private:
    MotionSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    QnCachingCameraDataLoaderPtr m_loader;
    std::deque<QnTimePeriod> m_data; //< Reversed list.
    int m_totalCount = 0;
};

} // namespace desktop
} // namespace client
} // namespace nx

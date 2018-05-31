#pragma once

#include <nx/vms/api/data/layout_tour_data.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class QnLayoutTourManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnLayoutTourManager(QObject* parent = nullptr);
    virtual ~QnLayoutTourManager() override;

    const nx::vms::api::LayoutTourDataList& tours() const;
    void resetTours(const nx::vms::api::LayoutTourDataList& tours = {});

    nx::vms::api::LayoutTourData tour(const QnUuid& id) const;
    nx::vms::api::LayoutTourDataList tours(const QList<QnUuid>& ids) const;

    void addOrUpdateTour(const nx::vms::api::LayoutTourData& tour);
    void removeTour(const QnUuid& tourId);

signals:
    void tourAdded(const nx::vms::api::LayoutTourData& tour);
    void tourChanged(const nx::vms::api::LayoutTourData& tour);
    void tourRemoved(const QnUuid& tourId);

private:
    mutable QnMutex m_mutex;
    nx::vms::api::LayoutTourDataList m_tours;
};

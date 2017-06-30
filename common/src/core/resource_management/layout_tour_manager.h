#pragma once

#include <nx_ec/data/api_layout_tour_data.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class QnLayoutTourManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnLayoutTourManager(QObject* parent = nullptr);
    virtual ~QnLayoutTourManager() override;

    const ec2::ApiLayoutTourDataList& tours() const;
    void resetTours(const ec2::ApiLayoutTourDataList& tours = ec2::ApiLayoutTourDataList());

    ec2::ApiLayoutTourData tour(const QnUuid& id) const;

    void addOrUpdateTour(const ec2::ApiLayoutTourData& tour);
    void removeTour(const QnUuid& tourId);

signals:
    void tourAdded(const ec2::ApiLayoutTourData& tour);
    void tourChanged(const ec2::ApiLayoutTourData& tour);
    void tourRemoved(const QnUuid& tourId);

private:
    mutable QnMutex m_mutex;
    ec2::ApiLayoutTourDataList m_tours;
};

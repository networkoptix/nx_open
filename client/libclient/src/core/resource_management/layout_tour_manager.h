#pragma once

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

struct QnLayoutTourItem
{
    QnLayoutResourcePtr layout;
    int delayMs = 0;
};

using QnLayoutTourItemList = std::vector<QnLayoutTourItem>;

class QnLayoutTourManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnLayoutTourManager(QObject* parent = nullptr);
    virtual ~QnLayoutTourManager() override;

    const ec2::ApiLayoutTourDataList& tours() const;
    void resetTours(const ec2::ApiLayoutTourDataList& tours);

    const ec2::ApiLayoutTourData& tour(const QnUuid& id) const;
    QnLayoutTourItemList tourItems(const QnUuid& id) const;
    QnLayoutTourItemList tourItems(const ec2::ApiLayoutTourData& tour) const;

    void addOrUpdateTour(const ec2::ApiLayoutTourData& tour);
    void saveTour(const ec2::ApiLayoutTourData& tour);
private:
    mutable QnMutex m_mutex;
    ec2::ApiLayoutTourDataList m_tours;
};

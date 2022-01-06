// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/data/layout_tour_data.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class NX_VMS_COMMON_API QnLayoutTourManager: public QObject
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
    mutable nx::Mutex m_mutex;
    nx::vms::api::LayoutTourDataList m_tours;
};

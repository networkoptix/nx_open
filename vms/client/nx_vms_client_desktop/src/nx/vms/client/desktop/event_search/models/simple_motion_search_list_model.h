// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>

#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtGui/QRegion>

#include <camera/camera_data_manager.h>
#include <nx/vms/client/core/common/data/motion_selection.h>
#include <nx/vms/client/desktop/event_search/models/abstract_search_list_model.h>
#include <recording/time_period_list.h>

class QMenu;

namespace nx::vms::client::desktop {

/**
 * Motion search list model for current workbench navigator resource.
 */
class SimpleMotionSearchListModel: public AbstractSearchListModel
{
    Q_OBJECT
    Q_PROPERTY(bool filterEmpty READ isFilterEmpty NOTIFY filterRegionsChanged)

    using base_type = AbstractSearchListModel;
    using MotionSelection = nx::vms::client::core::MotionSelection;

public:
    explicit SimpleMotionSearchListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~SimpleMotionSearchListModel() override = default;

    MotionSelection filterRegions() const;
    void setFilterRegions(const MotionSelection& value);
    bool isFilterEmpty() const;
    Q_INVOKABLE void clearFilterRegions();

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

signals:
    void filterRegionsChanged();

protected:
    virtual void requestFetch() override;
    virtual bool isFilterDegenerate() const override;

    virtual void clearData() override;
    virtual void truncateToRelevantTimePeriod() override;
    virtual void truncateToMaximumCount() override;

private:
    void updateMotionPeriods(qint64 startTimeMs);
    QnTimePeriodList periods() const; //< Periods from the loader.

    QSharedPointer<QMenu> contextMenu(const QnTimePeriod& chunk) const;

private:
    core::CachingCameraDataLoaderPtr m_loader;
    MotionSelection m_filterRegions;
    std::deque<QnTimePeriod> m_data; //< From newer to older.
    bool m_fetchInProgress = false;
    int m_totalCount = 0;
};

} // namespace nx::vms::client::desktop

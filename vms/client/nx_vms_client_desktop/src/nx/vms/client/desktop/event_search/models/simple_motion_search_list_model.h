#pragma once

#include <deque>

#include <QtCore/QList>
#include <QtCore/QSharedPointer>
#include <QtGui/QRegion>

#include <camera/camera_data_manager.h>
#include <recording/time_period_list.h>

#include <nx/vms/client/desktop/event_search/models/abstract_search_list_model.h>

class QMenu;

namespace nx::vms::client::desktop {

/**
 * Motion search list model for current workbench navigator resource.
 */
class SimpleMotionSearchListModel: public AbstractSearchListModel
{
    Q_OBJECT
    using base_type = AbstractSearchListModel;

public:
    explicit SimpleMotionSearchListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~SimpleMotionSearchListModel() override = default;

    QList<QRegion> filterRegions() const; //< One region per channel.
    void setFilterRegions(const QList<QRegion>& value);
    bool isFilterEmpty() const;

    virtual bool isConstrained() const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

protected:
    virtual void requestFetch() override;
    virtual bool isFilterDegenerate() const;

    virtual void clearData() override;
    virtual void truncateToRelevantTimePeriod() override;
    virtual void truncateToMaximumCount() override;

private:
    void updateMotionPeriods(qint64 startTimeMs);
    QnTimePeriodList periods() const; //< Periods from the loader.

    QSharedPointer<QMenu> contextMenu(const QnTimePeriod& chunk) const;

private:
    QnCachingCameraDataLoaderPtr m_loader;
    QList<QRegion> m_filterRegions;
    std::deque<QnTimePeriod> m_data; //< From newer to older.
    bool m_fetchInProgress = false;
    int m_totalCount = 0;
};

} // namespace nx::vms::client::desktop

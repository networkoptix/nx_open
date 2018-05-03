#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/client/desktop/common/models/concatenation_list_model.h>
#include <nx/vms/event/event_fwd.h>

class QnSortFilterListModel;

namespace nx {
namespace client {
namespace desktop {

class AbstractEventListModel;
class BusyIndicatorModel;

class UnifiedAsyncSearchListModel: public ConcatenationListModel
{
    Q_OBJECT
    using base_type = ConcatenationListModel;

public:
    explicit UnifiedAsyncSearchListModel(AbstractEventListModel* sourceModel,
        QObject* parent = nullptr);

    virtual ~UnifiedAsyncSearchListModel() override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

    QString clientsideTextFilter() const;
    void setClientsideTextFilter(const QString& value);

    QnTimePeriod relevantTimePeriod() const;
    void setRelevantTimePeriod(const QnTimePeriod& value);

    bool isConstrained() const;

    int relevantCount() const;

private:
    AbstractEventListModel* const m_sourceModel = nullptr;
    QnSortFilterListModel* const m_filterModel = nullptr;
    BusyIndicatorModel* const m_busyIndicatorModel = nullptr;
    int m_previousRowCount = 0;
};

} // namespace desktop
} // namespace client
} // namespace nx

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

class AbstractAsyncSearchListModel;
class BusyIndicatorModel;

class UnifiedAsyncSearchListModel: public ConcatenationListModel
{
    Q_OBJECT
    using base_type = ConcatenationListModel;

public:
    explicit UnifiedAsyncSearchListModel(AbstractAsyncSearchListModel* sourceModel,
        QObject* parent = nullptr);

    virtual ~UnifiedAsyncSearchListModel() override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

    QString textFilter() const;
    void setTextFilter(const QString& value);

    QnTimePeriod selectedTimePeriod() const;
    void setSelectedTimePeriod(const QnTimePeriod& value);

    bool isConstrained() const;

private:
    AbstractAsyncSearchListModel* const m_sourceModel = nullptr;
    QnSortFilterListModel* const m_filterModel = nullptr;
    BusyIndicatorModel* const m_busyIndicatorModel = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx

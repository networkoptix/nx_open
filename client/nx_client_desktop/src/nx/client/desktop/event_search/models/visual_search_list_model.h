#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>
#include <nx/client/desktop/common/models/concatenation_list_model.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class BusyIndicatorModel;

class VisualSearchListModel: public ConcatenationListModel
{
    Q_OBJECT
    using base_type = ConcatenationListModel;

public:
    explicit VisualSearchListModel(AbstractSearchListModel* sourceModel,
        QObject* parent = nullptr);

    virtual ~VisualSearchListModel() override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

    QnTimePeriod relevantTimePeriod() const;
    void setRelevantTimePeriod(const QnTimePeriod& value);

    using FetchDirection = AbstractSearchListModel::FetchDirection;
    FetchDirection fetchDirection() const;
    void setFetchDirection(FetchDirection value);

    bool isConstrained() const;

    int relevantCount() const;

private:
    AbstractSearchListModel* const m_sourceModel = nullptr;
    BusyIndicatorModel* const m_headIndicatorModel = nullptr;
    BusyIndicatorModel* const m_tailIndicatorModel = nullptr;
    int m_previousRowCount = 0;
};

} // namespace desktop
} // namespace client
} // namespace nx

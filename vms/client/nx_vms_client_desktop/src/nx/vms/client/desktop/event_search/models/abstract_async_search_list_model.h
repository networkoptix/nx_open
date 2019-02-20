#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/models/abstract_search_list_model.h>

namespace nx::vms::client::desktop {

/**
 * Abstract Right Panel data model that provides basic mechanics of sliding window asynchronous
 * lookup into underlying data store.
 */
class AbstractAsyncSearchListModel: public AbstractSearchListModel
{
    Q_OBJECT
    using base_type = AbstractSearchListModel;

protected:
    class Private;
    using CreatePrivate = std::function<Private*()>;

    nx::utils::ImplPtr<Private> d;

    explicit AbstractAsyncSearchListModel(
        QnWorkbenchContext* context, CreatePrivate dCreator, QObject* parent = nullptr);
public:
    virtual ~AbstractAsyncSearchListModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool fetchInProgress() const override;
    virtual bool cancelFetch() override;

protected:
    virtual bool canFetchNow() const override;
    virtual void requestFetch() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;
    virtual void clearData() override;
};

} // namespace nx::vms::client::desktop

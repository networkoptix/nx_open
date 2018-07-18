#pragma once

#include <functional>

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

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
    explicit AbstractAsyncSearchListModel(CreatePrivate dCreator, QObject* parent = nullptr);
    Private* d_func();

public:
    virtual ~AbstractAsyncSearchListModel() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool fetchInProgress() const override;
    virtual bool cancelFetch() override;

protected:
    virtual bool canFetch() const override;
    virtual void requestFetch() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;
    virtual void clearData() override;

private:
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

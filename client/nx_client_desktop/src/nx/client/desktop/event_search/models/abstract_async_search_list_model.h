#pragma once

#include <functional>

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

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

    void clear();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

    virtual bool fetchInProgress() const override;
    virtual bool cancelFetch() override;

    virtual QnTimePeriod fetchedTimeWindow() const override;

protected:
    virtual void relevantTimePeriodChanged(const QnTimePeriod& previousValue) override;
    virtual void fetchDirectionChanged() override;

private:
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

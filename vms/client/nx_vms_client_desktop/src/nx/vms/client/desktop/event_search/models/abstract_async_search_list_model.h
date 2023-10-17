// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>
#include <recording/time_period.h>

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
        WindowContext* context, CreatePrivate dCreator, QObject* parent = nullptr);
public:
    virtual ~AbstractAsyncSearchListModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool fetchInProgress() const override;
    virtual bool cancelFetch() override;

    bool fetchWindow(const QnTimePeriod& window);

signals:
    void asyncFetchStarted(nx::vms::client::desktop::RightPanel::FetchDirection direction,
        QPrivateSignal);

protected:
    virtual bool canFetchNow() const override;
    virtual void requestFetch() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;
    virtual void clearData() override;
};

} // namespace nx::vms::client::desktop

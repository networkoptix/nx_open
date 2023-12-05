// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/abstract_search_list_model.h>

namespace nx::vms::client::core {

/**
 * Abstract data model that provides basic mechanics of sliding window asynchronous
 * lookup into underlying data store.
 */
class NX_VMS_CLIENT_CORE_API AbstractAsyncSearchListModel: public AbstractSearchListModel
{
    Q_OBJECT
    using base_type = AbstractSearchListModel;

public:
    virtual ~AbstractAsyncSearchListModel() override;

    Q_INVOKABLE virtual bool fetchData(const FetchRequest& request) override;
    Q_INVOKABLE virtual bool cancelFetch() override;

    OptionalFetchRequest currentRequest() const;

protected:
    explicit AbstractAsyncSearchListModel(
        int maximumCount,
        SystemContext* systemContext,
        QObject* parent = nullptr);

    explicit AbstractAsyncSearchListModel(
        SystemContext* systemContext,
        QObject* parent = nullptr);

signals:
    void asyncFetchStarted(const FetchRequest& request, QPrivateSignal);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIdentityProxyModel>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>

Q_MOC_INCLUDE("nx/vms/client/core/event_search/models/fetch_request.h")

namespace nx::vms::client::core {

class AbstractAsyncSearchListModel;
struct FetchRequest;

/**
 * TODO: #ynikitenkov Make parent for RightPanelModelAdapter model.
 */
class NX_VMS_CLIENT_CORE_API EventSearchModelAdapter: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    static void registerQmlType();

    Q_INVOKABLE bool fetchInProgress() const;
    Q_INVOKABLE bool fetchData(const FetchRequest& request);
    Q_INVOKABLE void clear();
    Q_INVOKABLE FetchRequest requestForDirection(EventSearch::FetchDirection direction) const;

    void setSearchModel(AbstractAsyncSearchListModel* value);

    AbstractAsyncSearchListModel* searchModel() const;

private:
    using base_type::setSourceModel;

signals:
    void asyncFetchStarted(const FetchRequest& request);
    void fetchFinished(
        EventSearch::FetchResult result,
        int centralItemIndex,
        const FetchRequest& request);

private:
    nx::utils::ScopedConnections m_modelConnections;
};

} // namespace nx::vms::client::core

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/event_search/models/abstract_async_search_list_model.h>

namespace nx::vms::client::desktop {

class BookmarkSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit BookmarkSearchListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~BookmarkSearchListModel() override = default;

    virtual TextFilterSetup* textFilter() const override;

    // Clients do not receive any live updates from the server, therefore time periods of interest
    // must be periodically polled for updates by calling this method.
    void dynamicUpdate(const QnTimePeriod& period);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

private:
    class Private;
    Private* const d;
};

} // namespace nx::vms::client::desktop

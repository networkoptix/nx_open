// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/event_search/models/bookmark_search_list_model.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API BookmarkSearchListModel: public core::BookmarkSearchListModel
{
    Q_OBJECT
    using base_type = core::BookmarkSearchListModel;

public:
    using base_type::base_type;

    virtual QVariant data(const QModelIndex& index, int role) const override;
};

} // namespace nx::vms::client::desktop

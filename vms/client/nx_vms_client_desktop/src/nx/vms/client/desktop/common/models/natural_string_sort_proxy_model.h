// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/models/customizable_sort_filter_proxy_model.h>

namespace nx::vms::client::desktop {

class NaturalStringSortProxyModel: public CustomizableSortFilterProxyModel
{
    Q_OBJECT
    using base_type = CustomizableSortFilterProxyModel;

public:
    NaturalStringSortProxyModel(QObject* parent = nullptr);
};

} // namespace nx::vms::client::desktop

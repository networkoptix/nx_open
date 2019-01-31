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

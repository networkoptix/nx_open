#pragma once

#include <nx/client/desktop/common/models/customizable_sort_filter_proxy_model.h>

namespace nx {
namespace client {
namespace desktop {

class NaturalStringSortProxyModel: public CustomizableSortFilterProxyModel
{
    Q_OBJECT
    using base_type = CustomizableSortFilterProxyModel;

public:
    NaturalStringSortProxyModel(QObject* parent = nullptr);
};

} // namespace desktop
} // namespace client
} // namespace nx

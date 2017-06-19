#pragma once

#include <nx/client/desktop/ui/common/customizable_sort_filter_proxy_model.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class NaturalStringSortProxyModel: public CustomizableSortFilterProxyModel
{
    Q_OBJECT
    using base_type = CustomizableSortFilterProxyModel;

public:
    NaturalStringSortProxyModel(QObject* parent = nullptr);
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

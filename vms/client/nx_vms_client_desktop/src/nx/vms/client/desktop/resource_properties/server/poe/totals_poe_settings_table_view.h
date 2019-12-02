#pragma once

#include <nx/vms/client/desktop/node_view/node_view/table_node_view.h>

namespace nx::vms::client::desktop {

class TotalsPoeSettingsTableView: public node_view::TableNodeView
{
    Q_OBJECT
    using base_type = node_view::TableNodeView;

public:
    TotalsPoeSettingsTableView(QWidget* parent = nullptr);
    virtual ~TotalsPoeSettingsTableView() override;
};

} // namespace nx::vms::client::desktop

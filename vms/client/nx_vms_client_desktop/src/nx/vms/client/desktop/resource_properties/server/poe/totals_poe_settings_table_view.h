#pragma once

#include <nx/vms/client/desktop/node_view/node_view/table_node_view.h>

namespace nx::vms::client::desktop {
namespace settings {

class TotalsPoESettingsTableView: public node_view::TableNodeView
{
    Q_OBJECT
    using base_type = node_view::TableNodeView;

public:
    TotalsPoESettingsTableView(QWidget* parent = nullptr);
    virtual ~TotalsPoESettingsTableView() override;
};

} // namespace settings
} // namespace nx::vms::client::desktop

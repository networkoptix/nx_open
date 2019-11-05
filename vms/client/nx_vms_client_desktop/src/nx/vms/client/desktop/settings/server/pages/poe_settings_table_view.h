#pragma once

#include <nx/vms/client/desktop/node_view/node_view/table_node_view.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {
namespace settings {

enum PoESettingsColumn
{
    first,

    port = first,
    camera,
    consumption,
    speed,
    status,
    power,

    count
};

class PoESettingsTableView: public node_view::TableNodeView
{
    Q_OBJECT
    using base_type = node_view::TableNodeView;

public:
    PoESettingsTableView(QWidget* parent = nullptr);
    virtual ~PoESettingsTableView() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace settings
} // namespace nx::vms::client::desktop

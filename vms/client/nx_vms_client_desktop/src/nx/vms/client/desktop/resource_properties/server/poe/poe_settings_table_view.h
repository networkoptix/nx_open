#pragma once

#include <nx/vms/client/desktop/node_view/node_view/table_node_view.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

enum PoeSettingsColumn
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

class PoeSettingsTableView: public node_view::TableNodeView
{
    Q_OBJECT
    using base_type = node_view::TableNodeView;

public:
    PoeSettingsTableView(QWidget* parent = nullptr);
    virtual ~PoeSettingsTableView() override;

    static void setupPoeHeader(QTableView* view);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

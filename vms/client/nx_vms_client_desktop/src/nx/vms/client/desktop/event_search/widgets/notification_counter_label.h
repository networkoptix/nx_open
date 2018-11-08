#pragma once

#include <nx/vms/client/desktop/common/widgets/round_panel_label.h>

namespace nx::vms::client::desktop {

class NotificationCounterLabel: public RoundPanelLabel
{
    Q_OBJECT
    using base_type = RoundPanelLabel;

public:
    explicit NotificationCounterLabel(QWidget* parent = nullptr);
    virtual ~NotificationCounterLabel() override = default;
};

} // namespace nx::vms::client::desktop

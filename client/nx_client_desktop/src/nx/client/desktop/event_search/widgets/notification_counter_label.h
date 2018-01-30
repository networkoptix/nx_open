#pragma once

#include <nx/client/desktop/common/widgets/round_panel_label.h>

namespace nx {
namespace client {
namespace desktop {

class NotificationCounterLabel: public RoundPanelLabel
{
    Q_OBJECT
    using base_type = RoundPanelLabel;

public:
    explicit NotificationCounterLabel(QWidget* parent = nullptr);
    virtual ~NotificationCounterLabel() override = default;
};

} // namespace desktop
} // namespace client
} // namespace nx

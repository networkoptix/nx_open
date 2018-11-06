#pragma once

#include <nx/vms/client/desktop/common/widgets/hover_button.h>

namespace nx::vms::client::desktop {

/**
 * X button
 */
class CloseButton: public HoverButton
{
    Q_OBJECT
    using base_type = HoverButton;

public:
    CloseButton(QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop

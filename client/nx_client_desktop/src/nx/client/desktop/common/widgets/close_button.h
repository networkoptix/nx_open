#pragma once

#include <nx/client/desktop/common/widgets/hover_button.h>

namespace nx {
namespace client {
namespace desktop {

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

} // namespace desktop
} // namespace client
} // namespace nx

#include "parameter_helper.h"

#include <QtCore/QPointer>

#include <client/client_globals.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

namespace nx {
namespace client {
namespace desktop {
namespace utils {

WidgetPtr extractParentWidget(
    const nx::client::desktop::ui::action::Parameters& parameters,
    QWidget* defaultValue)
{
    return parameters.argument<WidgetPtr>(Qn::ParentWidgetRole, defaultValue);
}

} // namespace utils
} // namespace desktop
} // namespace client
} // namespace nx

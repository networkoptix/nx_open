#include "parameter_helper.h"

#include <QtCore/QPointer>

#include <client/client_globals.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>

namespace nx::vms::client::desktop {
namespace utils {

WidgetPtr extractParentWidget(
    const ui::action::Parameters& parameters,
    QWidget* defaultValue)
{
    return parameters.argument<WidgetPtr>(Qn::ParentWidgetRole, defaultValue);
}

} // namespace utils
} // namespace nx::vms::client::desktop

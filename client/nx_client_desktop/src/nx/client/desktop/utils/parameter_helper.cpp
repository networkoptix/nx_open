#include "parameter_helper.h"

#include <QtCore/QPointer>

#include <client/client_globals.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

namespace nx {
namespace utils {

WidgetPtr extractParentWidget(
    const client::desktop::ui::action::Parameters& parameters,
    QWidget* defaultValue)
{
    return parameters.argument<WidgetPtr>(Qn::ParentWidgetRole, defaultValue);
}

} // namespace utils
} // namespace nx

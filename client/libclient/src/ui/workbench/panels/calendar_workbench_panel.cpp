#include "calendar_workbench_panel.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/scoped_value_rollback.h>

namespace NxUi {

CalendarWorkbenchPanel::CalendarWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent)
{
}

bool CalendarWorkbenchPanel::isPinned() const
{
    return true;
}

bool CalendarWorkbenchPanel::isOpened() const
{
    return true;
}

void CalendarWorkbenchPanel::setOpened(bool opened, bool animate)
{
}

bool CalendarWorkbenchPanel::isVisible() const
{
    return true;
}

void CalendarWorkbenchPanel::setVisible(bool visible, bool animate)
{
}

qreal CalendarWorkbenchPanel::opacity() const
{
    return 1.0;
}

void CalendarWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{

}

bool CalendarWorkbenchPanel::isHovered() const
{
    return true;
}


} //namespace NxUi

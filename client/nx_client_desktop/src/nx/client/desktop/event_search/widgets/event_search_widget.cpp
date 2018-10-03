#include "event_search_widget.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QVBoxLayout>

#include <ui/style/helper.h>
#include <ui/style/skin.h>

#include <nx/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::client::desktop {

EventSearchWidget::EventSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new EventSearchListModel(context), parent),
    m_typeSelectionButton(createCustomFilterButton())
{
    setRelevantControls(Control::cameraSelector | Control::timeSelector | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/events.png"));

    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/event_rules.png"));
    m_typeSelectionButton->setSelectable(false);
    m_typeSelectionButton->setDeactivatable(true);

    auto eventFilterMenu = new QMenu(this);
    eventFilterMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    eventFilterMenu->setWindowFlags(eventFilterMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    auto addMenuAction =
        [this, eventFilterMenu](const QString& title, vms::api::EventType type)
        {
            auto action = eventFilterMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, title, type]()
                {
                    m_typeSelectionButton->setText(title);
                    m_typeSelectionButton->setState(type == vms::api::EventType::undefinedEvent
                        ? SelectableTextButton::State::deactivated
                        : SelectableTextButton::State::unselected);

                    auto eventModel = qobject_cast<EventSearchListModel*>(model());
                    if (!eventModel)
                        return;

                    eventModel->setSelectedEventType(type);
                    requestFetch();
                });

            return action;
        };

    nx::vms::event::StringsHelper helper(commonModule());

    auto defaultAction = addMenuAction(tr("Any type"), vms::api::EventType::undefinedEvent);
    for (const auto type: vms::event::allEvents())
    {
        if (vms::event::isSourceCameraRequired(type))
            addMenuAction(helper.eventName(type), type);
    }

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    m_typeSelectionButton->setMenu(eventFilterMenu);
}

void EventSearchWidget::resetFilters()
{
    base_type::resetFilters();
    m_typeSelectionButton->deactivate();
}

QString EventSearchWidget::placeholderText(bool constrained) const
{
    return constrained ? tr("No bookmarks") : tr("No events occured");
}

QString EventSearchWidget::itemCounterText(int count) const
{
    return tr("%n events", "", count);
}

} // namespace nx::client::desktop

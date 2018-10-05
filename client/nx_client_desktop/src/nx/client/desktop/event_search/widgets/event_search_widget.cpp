#include "event_search_widget.h"

#include <QtCore/QPointer>
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

// ------------------------------------------------------------------------------------------------
// EventSearchWidget::Private

class EventSearchWidget::Private
{
    EventSearchWidget* const q;
    SelectableTextButton* const m_typeSelectionButton;
    EventSearchListModel* const m_eventModel;

public:
    Private(EventSearchWidget* q);
    void resetType();

private:
    QAction* addMenuAction(QMenu* menu, const QString& title, vms::api::EventType type,
        bool dynamicTitle = false);

private:
    QAction* m_serverEventsSubmenuAction = nullptr;
};

EventSearchWidget::Private::Private(EventSearchWidget* q):
    q(q),
    m_typeSelectionButton(q->createCustomFilterButton()),
    m_eventModel(qobject_cast<EventSearchListModel*>(q->model()))
{
    NX_ASSERT(m_eventModel);

    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/event_rules.png"));
    m_typeSelectionButton->setSelectable(false);
    m_typeSelectionButton->setDeactivatable(true);

    auto eventFilterMenu = q->createMenu();
    auto deviceIssuesMenu = q->createMenu();
    auto serverEventsMenu = q->createMenu();

    deviceIssuesMenu->setTitle("<device issues>");
    serverEventsMenu->setTitle(tr("Server events"));

    nx::vms::event::StringsHelper helper(q->commonModule());

    for (const auto type: vms::event::allEvents())
    {
        if (vms::event::parentEvent(type) == vms::api::EventType::anyEvent)
            addMenuAction(eventFilterMenu, helper.eventName(type), type);
    }

    for (const auto type: vms::event::childEvents(vms::api::EventType::anyCameraEvent))
        addMenuAction(deviceIssuesMenu, helper.eventName(type), type);

    deviceIssuesMenu->addSeparator();
    q->addDeviceDependentAction(
        addMenuAction(deviceIssuesMenu, "<any device issue>", vms::api::EventType::anyCameraEvent),
        tr("Any device issue"), tr("Any camera issue"));

    for (const auto type: vms::event::childEvents(vms::api::EventType::anyServerEvent))
        addMenuAction(serverEventsMenu, helper.eventName(type), type);

    serverEventsMenu->addSeparator();
    addMenuAction(serverEventsMenu, tr("Any server event"), vms::api::EventType::anyServerEvent);

    eventFilterMenu->addSeparator();
    q->addDeviceDependentAction(eventFilterMenu->addMenu(deviceIssuesMenu),
        tr("Device issues"), tr("Camera issues"));

    m_serverEventsSubmenuAction = eventFilterMenu->addMenu(serverEventsMenu);
    eventFilterMenu->addSeparator();

    auto defaultAction = addMenuAction(
        eventFilterMenu, tr("Any event"), vms::api::EventType::undefinedEvent);

    QObject::connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, q,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    m_typeSelectionButton->setMenu(eventFilterMenu);

    // Disable server event selection when selected cameras differ from "Any camera".
    QObject::connect(q, &AbstractSearchWidget::cameraSetChanged,
        [this]()
        {
            const bool serverEventsVisible = this->q->selectedCameras() == Cameras::all;
            m_serverEventsSubmenuAction->setEnabled(serverEventsVisible);

            const bool isServerEvent = (vms::event::parentEvent(m_eventModel->selectedEventType())
                == vms::api::EventType::anyServerEvent);

            if (isServerEvent)
                resetType();
        });
}

void EventSearchWidget::Private::resetType()
{
    m_typeSelectionButton->deactivate();
}

QAction* EventSearchWidget::Private::addMenuAction(
    QMenu* menu, const QString& title, vms::api::EventType type, bool dynamicTitle)
{
    auto action = menu->addAction(title);
    QObject::connect(action, &QAction::triggered, q,
        [this, action, type]()
        {
            m_typeSelectionButton->setText(action->text());
            m_typeSelectionButton->setState(type == vms::api::EventType::undefinedEvent
                ? SelectableTextButton::State::deactivated
                : SelectableTextButton::State::unselected);

            m_eventModel->setSelectedEventType(type);
            q->requestFetch();
        });

    if (dynamicTitle)
    {
        QObject::connect(action, &QAction::changed, q,
            [this, action, type]()
            {
                if (m_eventModel->selectedEventType() == type)
                    m_typeSelectionButton->setText(action->text());
            });
    }

    return action;
}

// ------------------------------------------------------------------------------------------------
// EventSearchWidget

EventSearchWidget::EventSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new EventSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(Control::cameraSelector | Control::timeSelector | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/events.png"));
}

EventSearchWidget::~EventSearchWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void EventSearchWidget::resetFilters()
{
    base_type::resetFilters();
    d->resetType();
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

#include "event_search_widget.h"

#include <QtCore/QPointer>
#include <QtWidgets/QAction>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
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

    using EventType = nx::vms::api::EventType;

public:
    Private(EventSearchWidget* q);
    void resetType();

private:
    QAction* addMenuAction(QMenu* menu, const QString& title, EventType type,
        const QString& subType = QString(), bool dynamicTitle = false);

    void setupTypeSelection();
    void updateAnalyticsMenu();

private:
    QAction* m_serverEventsSubmenuAction = nullptr;

    QAction* m_analyticsEventsSubmenuAction = nullptr;
    QAction* m_analyticsEventsSingleAction = nullptr;
};

EventSearchWidget::Private::Private(EventSearchWidget* q):
    q(q),
    m_typeSelectionButton(q->createCustomFilterButton()),
    m_eventModel(qobject_cast<EventSearchListModel*>(q->model()))
{
    NX_ASSERT(m_eventModel);

    setupTypeSelection();

    // Update analytics events submenu when servers are added to or removed from the system.
    const auto handleServerChanges =
        [this](const QnResourcePtr& resource)
        {
            if (!m_eventModel->isOnline())
                return;

            const auto flags = resource->flags();
            if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake))
                updateAnalyticsMenu();
        };

    QObject::connect(q->resourcePool(), &QnResourcePool::resourceAdded, q, handleServerChanges);
    QObject::connect(q->resourcePool(), &QnResourcePool::resourceRemoved, q, handleServerChanges);

    QObject::connect(m_eventModel, &AbstractSearchListModel::isOnlineChanged, q,
        [this](bool isOnline)
        {
            if (isOnline)
                updateAnalyticsMenu();
        });

    if (m_eventModel->isOnline())
        updateAnalyticsMenu();

    // Disable server event selection when selected cameras differ from "Any camera".
    QObject::connect(q, &AbstractSearchWidget::cameraSetChanged,
        [this]()
        {
            NX_ASSERT(m_serverEventsSubmenuAction);

            const bool serverEventsVisible = this->q->selectedCameras() == Cameras::all;
            m_serverEventsSubmenuAction->setEnabled(serverEventsVisible);

            const bool isServerEvent = nx::vms::event::parentEvent(
                m_eventModel->selectedEventType()) == EventType::anyServerEvent;

            if (isServerEvent)
                resetType();
        });
}

void EventSearchWidget::Private::resetType()
{
    m_typeSelectionButton->deactivate();
}

void EventSearchWidget::Private::setupTypeSelection()
{
    using namespace nx::vms::event;

    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/event_rules.png"));
    m_typeSelectionButton->setSelectable(false);
    m_typeSelectionButton->setDeactivatable(true);

    auto eventFilterMenu = q->createDropdownMenu();
    auto deviceIssuesMenu = q->createDropdownMenu();
    auto serverEventsMenu = q->createDropdownMenu();
    auto analyticsEventsMenu = q->createDropdownMenu();

    deviceIssuesMenu->setTitle("<device issues>");
    serverEventsMenu->setTitle(tr("Server events"));
    analyticsEventsMenu->setTitle(tr("Analytics events"));

    StringsHelper helper(q->commonModule());

    m_analyticsEventsSubmenuAction = eventFilterMenu->addMenu(analyticsEventsMenu);
    m_analyticsEventsSubmenuAction->setVisible(false);

    for (const auto type: allEvents())
    {
        if (parentEvent(type) == EventType::anyEvent && type != EventType::analyticsSdkEvent)
            addMenuAction(eventFilterMenu, helper.eventName(type), type);
    }

    m_analyticsEventsSingleAction = addMenuAction(eventFilterMenu,
        helper.eventName(EventType::analyticsSdkEvent), EventType::analyticsSdkEvent);

    for (const auto type: childEvents(EventType::anyCameraEvent))
        addMenuAction(deviceIssuesMenu, helper.eventName(type), type);

    deviceIssuesMenu->addSeparator();

    q->addDeviceDependentAction(
        addMenuAction(deviceIssuesMenu, "<any device issue>", vms::api::EventType::anyCameraEvent,
            QString(), true),
        tr("Any device issue"),
        tr("Any camera issue"));

    for (const auto type: childEvents(EventType::anyServerEvent))
        addMenuAction(serverEventsMenu, helper.eventName(type), type);

    serverEventsMenu->addSeparator();
    addMenuAction(serverEventsMenu, tr("Any server event"), EventType::anyServerEvent);

    eventFilterMenu->addSeparator();
    q->addDeviceDependentAction(eventFilterMenu->addMenu(deviceIssuesMenu),
        tr("Device issues"), tr("Camera issues"));

    m_serverEventsSubmenuAction = eventFilterMenu->addMenu(serverEventsMenu);
    eventFilterMenu->addSeparator();

    auto defaultAction = addMenuAction(
        eventFilterMenu, tr("Any event"), EventType::undefinedEvent);

    QObject::connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, q,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    m_typeSelectionButton->setMenu(eventFilterMenu);
}

QAction* EventSearchWidget::Private::addMenuAction(QMenu* menu, const QString& title,
    EventType type, const QString& subType, bool dynamicTitle)
{
    auto action = menu->addAction(title);
    QObject::connect(action, &QAction::triggered, q,
        [this, action, type]()
        {
            m_typeSelectionButton->setText(action->text());
            m_typeSelectionButton->setState(type == EventType::undefinedEvent
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

void EventSearchWidget::Private::updateAnalyticsMenu()
{
    NX_ASSERT(m_analyticsEventsSubmenuAction && m_analyticsEventsSingleAction);

    auto analyticsMenu = m_analyticsEventsSubmenuAction->menu();
    NX_ASSERT(analyticsMenu);

    if (analyticsMenu)
    {
        // TODO: #vkutin Finish this after plugin manifest naming refactor is complete.
    }

    const bool hasAnalyticsMenu = analyticsMenu && !analyticsMenu->isEmpty();
    m_analyticsEventsSubmenuAction->setVisible(hasAnalyticsMenu);
    m_analyticsEventsSingleAction->setVisible(!hasAnalyticsMenu);

    // TODO: #vkutin If currently selected subtype is no longer available, clear it.
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

#include "event_search_widget.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include <client/client_runtime_settings.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/utils/string.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// EventSearchWidget::Private

class EventSearchWidget::Private: public QObject
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

    using EventTypeDescriptors = std::map<QString, nx::vms::api::analytics::EventTypeDescriptor>;
    struct EngineInfo
    {
        QString name;
        EventTypeDescriptors eventTypes;

        bool operator<(const EngineInfo& other) const
        {
            QCollator collator;
            collator.setNumericMode(true);
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            return collator.compare(name, other.name) < 0;
        }
    };
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

    connect(q->resourcePool(), &QnResourcePool::resourceAdded, this, handleServerChanges);
    connect(q->resourcePool(), &QnResourcePool::resourceRemoved, this, handleServerChanges);

    connect(m_eventModel, &AbstractSearchListModel::isOnlineChanged, this,
        [this](bool isOnline)
        {
            if (isOnline)
                updateAnalyticsMenu();
        });

    if (m_eventModel->isOnline())
        updateAnalyticsMenu();

    // Disable server event selection when selected cameras differ from "Any camera".
    connect(q, &AbstractSearchWidget::cameraSetChanged, this,
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

    connect(q->accessController(), &QnWorkbenchAccessController::globalPermissionsChanged,
        q, &EventSearchWidget::updateAllowance);
}

void EventSearchWidget::Private::resetType()
{
    m_typeSelectionButton->deactivate();
}

void EventSearchWidget::Private::setupTypeSelection()
{
    using namespace nx::vms::event;

    m_typeSelectionButton->setIcon(qnSkin->icon("text_buttons/event_rules.png"));

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

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
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
    connect(action, &QAction::triggered, this,
        [this, action, type, subType]()
        {
            m_typeSelectionButton->setText(action->text());
            m_typeSelectionButton->setState(type == EventType::undefinedEvent
                ? SelectableTextButton::State::deactivated
                : SelectableTextButton::State::unselected);

            m_eventModel->setSelectedEventType(type);
            m_eventModel->setSelectedSubType(subType);
        });

    if (dynamicTitle)
    {
        connect(action, &QAction::changed, this,
            [this, action, type, subType]()
            {
                if (m_eventModel->selectedEventType() == type
                    && m_eventModel->selectedSubType() == subType)
                {
                    m_typeSelectionButton->setText(action->text());
                }
            });
    }

    return action;
}

void EventSearchWidget::Private::updateAnalyticsMenu()
{
    NX_ASSERT(m_analyticsEventsSubmenuAction && m_analyticsEventsSingleAction);

    auto analyticsMenu = m_analyticsEventsSubmenuAction->menu();
    NX_ASSERT(analyticsMenu);

    const QString currentSelection = m_eventModel->selectedSubType();
    bool currentSelectionStillAvailable = false;

    if (analyticsMenu)
    {
        using namespace nx::vms::event;

        nx::analytics::EventTypeDescriptorManager eventTypeDescriptorManager(q->commonModule());
        nx::analytics::EngineDescriptorManager engineDescriptorManager(q->commonModule());
        const auto eventTypeDescriptors = eventTypeDescriptorManager.descriptors();
        const auto engineDescriptors = engineDescriptorManager.descriptors();
        analyticsMenu->clear();

        if (!eventTypeDescriptors.empty())
        {
            QHash<QnUuid, EngineInfo> engineById;
            for (const auto& [engineId, engineDescriptor]: engineDescriptors)
                engineById[engineId].name = engineDescriptor.name;

            for (const auto& [eventTypeId, eventTypeDescriptor]: eventTypeDescriptors)
            {
                for (const auto& scope: eventTypeDescriptor.scopes)
                {
                    engineById[scope.engineId].eventTypes
                        .emplace(eventTypeId, eventTypeDescriptor);
                }
            }

            QList<EngineInfo> engines;
            for (const auto& engineInfo: engineById)
                engines.push_back(engineInfo);

            std::sort(engines.begin(), engines.end());
            const bool multipleEnginesArePresent = engines.size() > 1;

            QMenu* currentMenu = analyticsMenu;
            for (const auto& engine: engines)
            {
                if (multipleEnginesArePresent)
                {
                    const auto engineName = engine.name.isEmpty()
                        ? QString("<%1>").arg(tr("unnamed analytics engine"))
                        : engine.name;

                    currentMenu->setWindowFlags(
                        currentMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);
                    currentMenu = analyticsMenu->addMenu(engineName);
                }

                for (const auto& [eventTypeId, eventTypeDescriptor]: engine.eventTypes)
                {
                    addMenuAction(currentMenu,
                        eventTypeDescriptor.name,
                        EventType::analyticsSdkEvent, eventTypeDescriptor.id);

                    if (!currentSelectionStillAvailable
                        && currentSelection == eventTypeDescriptor.getId())
                    {
                        currentSelectionStillAvailable = true;
                    }
                }
            }

            analyticsMenu->addSeparator();
            addMenuAction(analyticsMenu, tr("Any analytics event"),
                EventType::analyticsSdkEvent, {});
        }
    }

    const bool hasAnalyticsMenu = analyticsMenu && !analyticsMenu->isEmpty();
    m_analyticsEventsSubmenuAction->setVisible(hasAnalyticsMenu);
    m_analyticsEventsSingleAction->setVisible(!hasAnalyticsMenu);

    if (!currentSelectionStillAvailable)
        m_eventModel->setSelectedSubType({});
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
    return constrained ? tr("No events") : tr("No events occured");
}

QString EventSearchWidget::itemCounterText(int count) const
{
    return tr("%n events", "", count);
}

bool EventSearchWidget::calculateAllowance() const
{
    return accessController()->hasGlobalPermission(vms::api::GlobalPermission::viewLogs);
}

} // namespace nx::vms::client::desktop

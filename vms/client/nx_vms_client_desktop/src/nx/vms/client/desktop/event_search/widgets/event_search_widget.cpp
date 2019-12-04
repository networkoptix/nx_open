#include "event_search_widget.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QHash>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include <client/client_runtime_settings.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/vms/client/desktop/analytics/analytics_entities_tree.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/client/desktop/event_rules/nvr_events_actions_access.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/strings_helper.h>

#include <nx/utils/pending_operation.h>
#include <nx/utils/string.h>
#include <nx/utils/std/algorithm.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// EventSearchWidget::Private

class EventSearchWidget::Private: public QObject
{
    Q_DECLARE_TR_FUNCTIONS(EventSearchWidget::Private)
    EventSearchWidget* const q;
    SelectableTextButton* const m_typeSelectionButton;
    EventSearchListModel* const m_eventModel;

    using EventType = nx::vms::api::EventType;

public:
    Private(EventSearchWidget* q);
    void resetType();

private:
    void setupTypeSelection();

    QAction* addMenuAction(
        QMenu* menu,
        const QString& title,
        EventType type,
        const QString& subType = QString(),
        bool dynamicTitle = false) const;

    QMenu* createDeviceIssuesMenu(QWidget* parent) const;
    QMenu* createServerEventsMenu(QWidget* parent) const;

    void updateServerEventsMenu();
    void updateAnalyticsMenu();

private:
    QAction* m_cameraIssuesSubmenuAction = nullptr;
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

    const auto updateServerEventsMenuIfNeeded =
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::server) && !resource->hasFlags(Qn::fake))
                updateServerEventsMenu();
        };

    connect(q->resourcePool(), &QnResourcePool::resourceAdded,
        this, updateServerEventsMenuIfNeeded);
    connect(q->resourcePool(), &QnResourcePool::resourceRemoved,
        this, updateServerEventsMenuIfNeeded);

    auto updateAnalyticsSubmenuOperation = new nx::utils::PendingOperation(
        [this]
        {
            if (m_eventModel->isOnline())
                updateAnalyticsMenu();
        },
        100ms,
        this);
    updateAnalyticsSubmenuOperation->fire();
    updateAnalyticsSubmenuOperation->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);

    connect(m_eventModel, &AbstractSearchListModel::isOnlineChanged,
        updateAnalyticsSubmenuOperation,
        &nx::utils::PendingOperation::requestOperation);

    connect(q->commonModule()->instance<AnalyticsEventsSearchTreeBuilder>(),
        &AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged,
        updateAnalyticsSubmenuOperation,
        &nx::utils::PendingOperation::requestOperation);

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

    connect(q->model(), &AbstractSearchListModel::isOnlineChanged,
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
    auto analyticsEventsMenu = q->createDropdownMenu();

    analyticsEventsMenu->setTitle(tr("Analytics events"));

    StringsHelper helper(q->commonModule());

    auto defaultAction = addMenuAction(
        eventFilterMenu, tr("Any event"), EventType::undefinedEvent);

    eventFilterMenu->addSeparator();

    m_analyticsEventsSubmenuAction = eventFilterMenu->addMenu(analyticsEventsMenu);
    m_analyticsEventsSubmenuAction->setVisible(false);

    for (const auto type: allEvents())
    {
        if (parentEvent(type) == EventType::anyEvent && type != EventType::analyticsSdkEvent)
            addMenuAction(eventFilterMenu, helper.eventName(type), type);
    }

    m_analyticsEventsSingleAction = addMenuAction(eventFilterMenu,
        helper.eventName(EventType::analyticsSdkEvent), EventType::analyticsSdkEvent);

    eventFilterMenu->addSeparator();
    q->addDeviceDependentAction(eventFilterMenu->addMenu(createDeviceIssuesMenu(eventFilterMenu)),
        tr("Device issues"), tr("Camera issues"));

    m_serverEventsSubmenuAction =
        eventFilterMenu->addMenu(createServerEventsMenu(eventFilterMenu));

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    m_typeSelectionButton->setMenu(eventFilterMenu);
}

QAction* EventSearchWidget::Private::addMenuAction(
    QMenu* menu,
    const QString& title,
    EventType type,
    const QString& subType,
    bool dynamicTitle) const
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

QMenu* EventSearchWidget::Private::createDeviceIssuesMenu(QWidget* parent) const
{
    auto menu = new QMenu(parent);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    q->addDeviceDependentAction(
        addMenuAction(menu, QString(), vms::api::EventType::anyCameraEvent, QString(), true),
        tr("Any device issue"),
        tr("Any camera issue"));

    menu->addSeparator();

    nx::vms::event::StringsHelper stringsHelper(q->commonModule());
    for (const auto type: nx::vms::event::childEvents(EventType::anyCameraEvent))
        addMenuAction(menu, stringsHelper.eventName(type), type);

    return menu;
}

QMenu* EventSearchWidget::Private::createServerEventsMenu(QWidget* parent) const
{
    auto menu = new QMenu(parent);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    menu->setTitle(tr("Server events"));

    addMenuAction(menu, tr("Any server event"), EventType::anyServerEvent);
    menu->addSeparator();

    const auto accessibleServerEvents = NvrEventsActionsAccess::removeInacessibleNvrEvents(
        nx::vms::event::childEvents(EventType::anyServerEvent), q->resourcePool());

    nx::vms::event::StringsHelper stringsHelper(q->commonModule());
    for (const auto type: accessibleServerEvents)
        addMenuAction(menu, stringsHelper.eventName(type), type);

    return menu;
}

void EventSearchWidget::Private::updateServerEventsMenu()
{
    m_typeSelectionButton->menu()->removeAction(m_serverEventsSubmenuAction);
    m_serverEventsSubmenuAction->menu()->deleteLater();
    m_serverEventsSubmenuAction->deleteLater();
    m_serverEventsSubmenuAction = m_typeSelectionButton->menu()->insertMenu(
        m_cameraIssuesSubmenuAction, createServerEventsMenu(m_typeSelectionButton->menu()));
}

void EventSearchWidget::Private::updateAnalyticsMenu()
{
    NX_ASSERT(m_analyticsEventsSubmenuAction && m_analyticsEventsSingleAction);

    auto analyticsMenu = m_analyticsEventsSubmenuAction->menu();

    const QString currentSelection = m_eventModel->selectedSubType();
    bool currentSelectionStillAvailable = false;

    auto addItemRecursive = nx::utils::y_combinator(
        [this, currentSelection, &currentSelectionStillAvailable]
        (auto addItemRecursive, auto parent, auto root) -> void
        {
            using NodeType = AnalyticsEntitiesTreeBuilder::NodeType;

            for (auto node: root->children)
            {
                // Leaf node - event type.
                if (node->nodeType == NodeType::eventType)
                {
                    addMenuAction(parent, node->text, EventType::analyticsSdkEvent,
                        node->entityId);

                    if (!currentSelectionStillAvailable
                        && currentSelection == node->entityId)
                    {
                        currentSelectionStillAvailable = true;
                    }
                }
                else
                {
                    NX_ASSERT(!node->children.empty());
                    auto menu = parent->addMenu(node->text);
                    q->fixMenuFlags(menu);

                    // TODO: Implement search by the analytics events groups: VMS-14877.
                    // if (child->nodeType == NodeType::group)
                    // {
                    //     addMenuAction(menu, child->name, EventType::analyticsSdkEvent,
                    //         groupId);
                    //     menu->addSeparator();
                    //  }

                    addItemRecursive(menu, node);
                }
            }
        });

    if (NX_ASSERT(analyticsMenu))
    {
        using namespace nx::vms::event;

        auto analyticsEventsSearchTreeBuilder =
            q->commonModule()->instance<AnalyticsEventsSearchTreeBuilder>();

        const auto root = analyticsEventsSearchTreeBuilder->eventTypesTree();

        WidgetUtils::clearMenu(analyticsMenu);

        if (!root->children.empty())
        {
            addMenuAction(
                analyticsMenu,
                tr("Any analytics event"),
                EventType::analyticsSdkEvent,
                {});

            analyticsMenu->addSeparator();
            addItemRecursive(analyticsMenu, root);
        }
    }

    const bool hasAnalyticsMenu = analyticsMenu && !analyticsMenu->isEmpty();
    m_analyticsEventsSubmenuAction->setVisible(hasAnalyticsMenu);
    m_analyticsEventsSingleAction->setVisible(!hasAnalyticsMenu);

    if (!currentSelectionStillAvailable)
        m_typeSelectionButton->deactivate();
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
    return constrained ? tr("No events") : tr("No events occurred");
}

QString EventSearchWidget::itemCounterText(int count) const
{
    return tr("%n events", "", count);
}

bool EventSearchWidget::calculateAllowance() const
{
    return model()->isOnline()
        && accessController()->hasGlobalPermission(vms::api::GlobalPermission::viewLogs);
}

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_search_widget.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QCollator>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QAction>
#include <QtWidgets/QMenu>

#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/rules/nvr_events_actions_access.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/strings.h>
#include <ui/workbench/workbench_context.h>

using namespace std::chrono;

namespace {

using nx::vms::client::core::AccessController;

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight16Theme = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light14"}},
    {QIcon::Selected, {.primary = "light14"}},
    {QnIcon::Pressed, {.primary = "light14"}},
};

NX_DECLARE_COLORIZED_ICON(kEventRulesIcon, "20x20/Outline/event_rules.svg", kLight16Theme)

nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kPlaceholderTheme = {
    {QnIcon::Normal, {.primary = "dark10"}}};

NX_DECLARE_COLORIZED_ICON(
    kEventsPlaceholderIcon, "64x64/Outline/events.svg", kPlaceholderTheme)

} // namespace

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
            if (resource->hasFlags(Qn::server))
                updateServerEventsMenu();
        };

    connect(q->system()->resourcePool(), &QnResourcePool::resourceAdded,
        this, updateServerEventsMenuIfNeeded);
    connect(q->system()->resourcePool(), &QnResourcePool::resourceRemoved,
        this, updateServerEventsMenuIfNeeded);

    connect(q->system()->saasServiceManager(),
        &nx::vms::common::saas::ServiceManager::dataChanged,
        this,
        &EventSearchWidget::Private::updateServerEventsMenu);

    connect(q->system()->globalSettings(),
        &nx::vms::common::SystemSettings::organizationIdChanged,
        this,
        &EventSearchWidget::Private::updateServerEventsMenu);

    const auto updateAnalyticsMenuIfEventModelIsOnline =
        [this]
        {
            if (m_eventModel->isOnline())
                updateAnalyticsMenu();
        };

    connect(m_eventModel, &core::AbstractSearchListModel::isOnlineChanged,
        this,
        updateAnalyticsMenuIfEventModelIsOnline);

    const auto builder = q->system()->analyticsEventsSearchTreeBuilder();
    connect(builder,
        &core::AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged,
        this,
        updateAnalyticsMenuIfEventModelIsOnline);

    updateAnalyticsMenuIfEventModelIsOnline();

    // Disable server event selection when selected cameras differ from "Any camera".
    connect(q, &AbstractSearchWidget::cameraSetChanged, this,
        [this]()
        {
            NX_ASSERT(m_serverEventsSubmenuAction);

            const bool serverEventsVisible =
                this->q->commonSetup()->cameraSelection() == core::EventSearch::CameraSelection::all;

            m_serverEventsSubmenuAction->setEnabled(serverEventsVisible);

            const bool isServerEvent = nx::vms::event::parentEvent(
                m_eventModel->selectedEventType()) == EventType::anyServerEvent;

            if (isServerEvent)
                resetType();
        });

    connect(q->system()->accessController(), &AccessController::globalPermissionsChanged,
        q, &EventSearchWidget::updateAllowance);

    connect(q->model(), &core::AbstractSearchListModel::isOnlineChanged,
        q, &EventSearchWidget::updateAllowance);
}

void EventSearchWidget::Private::resetType()
{
    m_typeSelectionButton->deactivate();
}

void EventSearchWidget::Private::setupTypeSelection()
{
    using namespace nx::vms::event;

    m_typeSelectionButton->setIcon(qnSkin->icon(kEventRulesIcon));

    auto eventFilterMenu = q->createDropdownMenu();
    auto analyticsEventsMenu = q->createDropdownMenu();

    analyticsEventsMenu->setTitle(rules::Strings::analyticsEvents());

    StringsHelper helper(q->system());

    auto defaultAction = addMenuAction(
        eventFilterMenu,
        rules::Strings::anyEvent(),
        EventType::undefinedEvent);

    eventFilterMenu->addSeparator();

    m_analyticsEventsSubmenuAction = eventFilterMenu->addMenu(analyticsEventsMenu);
    m_analyticsEventsSubmenuAction->setVisible(false);

    const auto supportedEventTypes = allEvents({
        isNonDeprecatedEvent,
        isApplicableForLicensingMode(q->system())});

    for (const auto type: supportedEventTypes)
    {
        if (parentEvent(type) == EventType::anyEvent && type != EventType::analyticsSdkEvent)
            addMenuAction(eventFilterMenu, helper.eventName(type), type);
    }

    m_analyticsEventsSingleAction = addMenuAction(eventFilterMenu,
        helper.eventName(EventType::analyticsSdkEvent), EventType::analyticsSdkEvent);

    eventFilterMenu->addSeparator();
    q->addDeviceDependentAction(eventFilterMenu->addMenu(createDeviceIssuesMenu(eventFilterMenu)),
        rules::Strings::deviceIssues_internal(),
        rules::Strings::cameraIssues_internal());

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
        rules::Strings::anyDeviceIssue_internal(),
        rules::Strings::anyCameraIssue_internal());

    menu->addSeparator();

    nx::vms::event::StringsHelper stringsHelper(q->system());
    for (const auto type: nx::vms::event::childEvents(EventType::anyCameraEvent))
        addMenuAction(menu, stringsHelper.eventName(type), type);

    return menu;
}

QMenu* EventSearchWidget::Private::createServerEventsMenu(QWidget* parent) const
{
    using namespace nx::vms::event;

    auto menu = new QMenu(parent);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    menu->setTitle(rules::Strings::serverEvents());

    addMenuAction(menu, rules::Strings::anyServerEvent(), EventType::anyServerEvent);
    menu->addSeparator();

    const auto supportedEventTypes = allEvents({
        isNonDeprecatedEvent,
        isApplicableForLicensingMode(q->system()),
        isChildOf(EventType::anyServerEvent)});

    const auto accessibleServerEvents = NvrEventsActionsAccess::removeInacessibleNvrEvents(
        supportedEventTypes, q->system()->resourcePool());

    nx::vms::event::StringsHelper stringsHelper(q->system());
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
            using NodeType = core::AnalyticsEntitiesTreeBuilder::NodeType;

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

        const auto root = q->system()->analyticsEventsSearchTreeBuilder()->eventTypesTree();

        WidgetUtils::clearMenu(analyticsMenu);

        if (!root->children.empty())
        {
            addMenuAction(
                analyticsMenu,
                rules::Strings::anyAnalyticsEvent(),
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

EventSearchWidget::EventSearchWidget(WindowContext* context, QWidget* parent):
    base_type(context, new EventSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(
        Control::cameraSelector
        | Control::cameraSelectionDisplay
        | Control::timeSelector
        | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->icon(kEventsPlaceholderIcon).pixmap(64, 64));
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

QString EventSearchWidget::placeholderText(bool /*constrained*/) const
{
    return makePlaceholderText(tr("No events"),
        tr("Try changing the filters or create an Event Rule"));
}

QString EventSearchWidget::itemCounterText(int count) const
{
    return tr("%n events", "", count);
}

bool EventSearchWidget::calculateAllowance() const
{
    return model()->isOnline()
        && system()->accessController()->hasGlobalPermissions(GlobalPermission::viewLogs)
        && system()->vmsRulesEngine()->isOldEngineEnabled();
}

} // namespace nx::vms::client::desktop

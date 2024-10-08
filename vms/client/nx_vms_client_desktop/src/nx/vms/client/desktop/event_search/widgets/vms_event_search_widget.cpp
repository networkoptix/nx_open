// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_event_search_widget.h"

#include <algorithm>

#include <QtCore/QCollator>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtGui/QAction>
#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/models/vms_event_search_list_model.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/rules/nvr_events_actions_access.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/events/analytics_event.h>
#include <nx/vms/rules/group.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/compatibility.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::vms::rules;

using nx::vms::client::core::AccessController;

namespace {

static const auto kAnalyticsEventType = utils::type<AnalyticsEvent>();

static const QColor kLight12Color = "#91A7B2";
static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight12Color, "light12"}, {kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight12Color, "light13"}, {kLight16Color, "light14"}}},
    {QIcon::Selected, {{kLight12Color, "light11"}, {kLight16Color, "light10"}}},
    {QnIcon::Pressed, {{kLight12Color, "light11"}, {kLight16Color, "light14"}}},
};

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight16Theme = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light14"}},
    {QIcon::Selected, {.primary = "light10"}},
    {QnIcon::Pressed, {.primary = "light14"}},
};

NX_DECLARE_COLORIZED_ICON(kEventRulesIcon, "20x20/Outline/event_rules.svg", kLight16Theme)

nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kPlaceholderTheme = {
    {QnIcon::Normal, {.primary = "dark10"}}};

NX_DECLARE_COLORIZED_ICON(
    kEventsPlaceholderIcon, "64x64/Outline/events.svg", kPlaceholderTheme)

} // namespace

// ------------------------------------------------------------------------------------------------
// VmsEventSearchWidget::Private

class VmsEventSearchWidget::Private: public QObject
{
    VmsEventSearchWidget* const q;
    SelectableTextButton* const m_typeSelectionButton;
    VmsEventSearchListModel* const m_eventModel;

public:
    Private(VmsEventSearchWidget* q);
    void resetType();

private:
    void setupTypeSelection();
    QMenu* createEventGroupMenu(const nx::vms::rules::Group& group);

    QAction* addMenuAction(
        QMenu* menu,
        const QString& title,
        const QString& type,
        const QString& subType = {},
        bool dynamicTitle = false) const;

    void updateServerEventsMenu();
    void updateAnalyticsMenu();

private:
    QAction* m_cameraIssuesSubmenuAction = nullptr;
    QAction* m_serverEventsSubmenuAction = nullptr;

    QAction* m_analyticsEventsSubmenuAction = nullptr;
    QAction* m_analyticsEventsSingleAction = nullptr;
};

VmsEventSearchWidget::Private::Private(VmsEventSearchWidget* q):
    q(q),
    m_typeSelectionButton(q->createCustomFilterButton()),
    m_eventModel(qobject_cast<VmsEventSearchListModel*>(q->model()))
{
    NX_ASSERT(m_eventModel);

    setupTypeSelection();

    const auto updateServerEventsMenuIfNeeded =
        [this](const QnResourceList& resources)
        {
            if (std::any_of(
                resources.begin(),
                resources.end(),
                [this](const QnResourcePtr& resource)
                {
                    return resource->hasFlags(Qn::server);
                }))
            {
                updateServerEventsMenu();
            }
        };

    connect(q->system()->resourcePool(), &QnResourcePool::resourcesAdded,
        this, updateServerEventsMenuIfNeeded);
    connect(q->system()->resourcePool(), &QnResourcePool::resourcesRemoved,
        this, updateServerEventsMenuIfNeeded);

    const auto updateAnalyticsMenuIfEventModelIsOnline =
        [this]
        {
            if (m_eventModel->isOnline())
                updateAnalyticsMenu();
        };

    connect(m_eventModel, &core::AbstractSearchListModel::isOnlineChanged,
        this,
        updateAnalyticsMenuIfEventModelIsOnline);

    connect(q->system()->analyticsEventsSearchTreeBuilder(),
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

            const bool isServerEvent =
                m_eventModel->selectedEventType() == kServerIssueEventGroup;

            if (isServerEvent)
                resetType();
        });

    connect(q->system()->accessController(), &AccessController::globalPermissionsChanged,
        q, &VmsEventSearchWidget::updateAllowance);

    connect(q->model(), &core::AbstractSearchListModel::isOnlineChanged,
        q, &VmsEventSearchWidget::updateAllowance);
}

void VmsEventSearchWidget::Private::resetType()
{
    m_typeSelectionButton->deactivate();
}

void VmsEventSearchWidget::Private::setupTypeSelection()
{
    using namespace nx::vms::event;

    m_typeSelectionButton->setIcon(qnSkin->icon(kEventRulesIcon));

    auto eventFilterMenu =
        createEventGroupMenu(rules::utils::filterEventGroups(
            q->system(),
            rules::utils::defaultItemFilters(),
            q->system()->vmsRulesEngine()->eventGroups()));
    auto defaultAction = eventFilterMenu->actions()[0];

    m_typeSelectionButton->setMenu(eventFilterMenu);

    connect(m_typeSelectionButton, &SelectableTextButton::stateChanged, this,
        [defaultAction](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
}

QMenu* VmsEventSearchWidget::Private::createEventGroupMenu(const Group& group)
{
    const auto groupHeaders = QMap<std::string, QString>{
        {"", rules::Strings::anyEvent()},
        {kDeviceIssueEventGroup, rules::Strings::deviceIssues(q->system())},
        {kServerIssueEventGroup, rules::Strings::serverEvents()},
    };

    auto result = q->createDropdownMenu();

    addMenuAction(result, group.name, QString::fromStdString(group.id));
    result->addSeparator();

    for (const auto& eventType: group.items)
    {
        auto eventAction = addMenuAction(
            result,
            rules::Strings::eventName(q->system(), eventType),
            eventType);

        // Creating analytics events submenu for active event types.
        if (eventType == rules::utils::type<AnalyticsEvent>())
        {
            m_analyticsEventsSingleAction = eventAction;

            auto analyticsEventsMenu = q->createDropdownMenu();
            analyticsEventsMenu->setTitle(rules::Strings::analyticsEvents());

            m_analyticsEventsSubmenuAction = result->addMenu(analyticsEventsMenu);
            m_analyticsEventsSubmenuAction->setVisible(false);
        }
    }

    result->addSeparator();

    for (const auto& subGroup : group.groups)
    {
        auto groupMenu = createEventGroupMenu(subGroup);

        if (subGroup.id == kServerIssueEventGroup)
            m_serverEventsSubmenuAction = groupMenu->menuAction();
        else if (subGroup.id == kDeviceIssueEventGroup)
            m_cameraIssuesSubmenuAction = groupMenu->menuAction();

        result->addMenu(groupMenu);
    }

    result->setTitle(groupHeaders.value(group.id));
    return result;
}

QAction* VmsEventSearchWidget::Private::addMenuAction(
    QMenu* menu,
    const QString& title,
    const QString& type,
    const QString& subType,
    bool dynamicTitle) const
{
    auto action = menu->addAction(title);
    connect(action, &QAction::triggered, this,
        [this, action, type, subType]()
        {
            m_typeSelectionButton->setText(action->text());
            m_typeSelectionButton->setState(type.isEmpty()
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

void VmsEventSearchWidget::Private::updateServerEventsMenu()
{
    NX_ASSERT(m_serverEventsSubmenuAction);
    auto serverMenu = m_serverEventsSubmenuAction->menu();

    const auto serverGroup = rules::utils::filterEventGroups(
        q->system(),
        rules::utils::defaultItemFilters(),
        *q->system()->vmsRulesEngine()->eventGroups().findGroup(kServerIssueEventGroup));

    WidgetUtils::clearMenu(serverMenu);

    // TODO: #amalov It may be easier just to hide unavailable events.
    auto newMenu = createEventGroupMenu(serverGroup);
    for (auto action : newMenu->actions())
    {
        action->setParent(serverMenu);
        serverMenu->addAction(action);
    }

    WidgetUtils::clearMenu(newMenu);
    newMenu->deleteLater();
}

void VmsEventSearchWidget::Private::updateAnalyticsMenu()
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
                    addMenuAction(parent, node->text, kAnalyticsEventType, node->entityId);

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
        const auto root = q->system()->analyticsEventsSearchTreeBuilder()->eventTypesTree();

        WidgetUtils::clearMenu(analyticsMenu);

        if (!root->children.empty())
        {
            addMenuAction(
                analyticsMenu,
                rules::Strings::anyAnalyticsEvent(),
                kAnalyticsEventType,
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
// VmsEventSearchWidget

VmsEventSearchWidget::VmsEventSearchWidget(WindowContext* context, QWidget* parent):
    base_type(context, new VmsEventSearchListModel(context), parent),
    d(new Private(this))
{
    setRelevantControls(
        Control::cameraSelector
        | Control::cameraSelectionDisplay
        | Control::timeSelector
        | Control::previewsToggler);
    setPlaceholderPixmap(qnSkin->icon(kEventsPlaceholderIcon).pixmap(64, 64));
}

VmsEventSearchWidget::~VmsEventSearchWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void VmsEventSearchWidget::resetFilters()
{
    base_type::resetFilters();
    d->resetType();
}

QString VmsEventSearchWidget::placeholderText(bool /*constrained*/) const
{
    return makePlaceholderText(tr("No events"),
        tr("Try changing the filters or create an Event Rule"));
}

QString VmsEventSearchWidget::itemCounterText(int count) const
{
    return tr("%n events", "", count);
}

bool VmsEventSearchWidget::calculateAllowance() const
{
    return nx::vms::rules::ini().fullSupport
        && model()->isOnline()
        && system()->accessController()->hasGlobalPermissions(GlobalPermission::viewLogs)
        && system()->vmsRulesEngine()->isEnabled();
}

} // namespace nx::vms::client::desktop

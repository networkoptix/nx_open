// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "right_panel_models_adapter.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <memory>

#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/string.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/visible_item_data_decorator_model.h>
#include <nx/vms/client/core/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/core/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/core/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/core/thumbnails/abstract_caching_resource_thumbnail.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/analytics/attribute_display_manager.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/event_rules/models/detectable_object_type_model.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/synchronizers/analytics_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/bookmark_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/motion_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/event_search/widgets/private/tile_interaction_handler_p.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/strings.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "analytics_event_model.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

// Set to true to see complete loaded item count instead of ">99".
constexpr bool kDebugShowFullItemCount = false;

/*
 * Tiles can have optional timed auto-close mode.
 * When such tile is first created, expiration timer is set to kInvisibleAutoCloseDelay.
 * Every time the tile becomes visible, remaining time is reset to kVisibleAutoCloseDelay.
 * When the tile becomes invisible, remaining time is not changed.
 * When the tile is hovered, the timer is ignored.
 * When the tile stops being hovered, remaining time is reset to kVisibleAutoCloseDelay.
 */
static constexpr milliseconds kVisibleAutoCloseDelay = 20s;
static constexpr milliseconds kInvisibleAutoCloseDelay = 80s;

static constexpr milliseconds kQueuedFetchDelay = 50ms;

// There's a lot of code duplication of preview loading mechanics with EventRibbon.
// TODO: #vkutin Refactor it.

static constexpr milliseconds kPreviewCheckInterval = 100ms;

static constexpr milliseconds kPreviewLoadTimeout = 1min;

static constexpr int kSimultaneousPreviewLoadsLimit = 15;
static constexpr int kSimultaneousPreviewLoadsLimitArm = 5;

static constexpr int kDefaultThumbnailWidth = 224;

static const auto kBypassVideoCachePropertyName = "__qn_bypassVideoCache";

// Load previews no more than one per this time period.
milliseconds tilePreviewLoadInterval()
{
    return milliseconds(ini().tilePreviewLoadIntervalMs);
}

// A delay after which preview may be requested again after receiving "NO DATA".
milliseconds previewReloadDelay()
{
    return seconds(ini().rightPanelPreviewReloadDelay);
}

int maxSimultaneousPreviewLoads(const QnMediaServerResourcePtr& server)
{
    if (QnMediaServerResource::isArmServer(server))
    {
        return std::clamp(ini().maxSimultaneousTilePreviewLoadsArm,
            1, kSimultaneousPreviewLoadsLimitArm);
    }

    return std::clamp(ini().maxSimultaneousTilePreviewLoads,
        1, kSimultaneousPreviewLoadsLimit);
}

QnMediaServerResourcePtr previewServer(const core::ResourceThumbnailProvider* provider)
{
    const auto resource = provider->resource();
    return resource
        ? resource->getParentResource().dynamicCast<QnMediaServerResource>()
        : QnMediaServerResourcePtr();
}

bool isBusy(core::ThumbnailStatus status)
{
    return status == core::ThumbnailStatus::Loading || status == core::ThumbnailStatus::Refreshing;
}

qlonglong providerId(core::ResourceThumbnailProvider* provider)
{
    return qlonglong(provider);
}

QHash<intptr_t, QPointer<core::ResourceThumbnailProvider>>& previewsById()
{
    static QHash<intptr_t, QPointer<core::ResourceThumbnailProvider>> data;
    return data;
}

core::analytics::AttributeList filterAndSortAttributeList(
    core::analytics::AttributeList attributes,
    const QStringList& attributeOrder,
    const QSet<QString>& visibleAttributes)
{
    core::analytics::AttributeList result;

    for (const QString& name: attributeOrder)
    {
        auto it = std::find_if(attributes.begin(), attributes.end(),
            [name](const core::analytics::Attribute& attribute) { return attribute.id == name; });

        if (it == attributes.end())
            continue;

        if (visibleAttributes.contains(it->id))
            result.append(*it);

        attributes.erase(it);
    }

    return result;
}

} // namespace

using nx::vms::client::core::analytics::taxonomy::AnalyticsFilterModel;

class RightPanelModelsAdapter::Private: public QObject
{
    RightPanelModelsAdapter* const q;

public:
    Private(RightPanelModelsAdapter* q);

    WindowContext* context() const { return m_context; }
    void setContext(WindowContext* value);

    core::EventSearch::SearchType type() const { return m_type; }
    void setType(core::EventSearch::SearchType value);

    bool isAllowed() const;

    bool active() const;
    void setActive(bool value);

    CommonObjectSearchSetup* commonSetup() const { return m_commonSetup; }
    core::AnalyticsSearchSetup* analyticsSetup() const { return m_analyticsSetup.get(); }
    QAbstractItemModel* analyticsEvents() const { return m_analyticsEvents.get(); }
    DetectableObjectTypeModel* objectTypeModel() const { return m_objectTypeModel; }

    void setAutoClosePaused(int row, bool value);

    void fetchData(const core::FetchRequest& request,
        bool immediately);
    bool fetchInProgress() const;

    bool isConstrained() const { return m_constrained; }
    bool isPlaceholderRequired() const { return m_isPlaceholderRequired; }
    int itemCount() const { return m_itemCount; }

    bool unreadTrackingEnabled() const { return m_unreadTrackingEnabled; }
    void setUnreadTrackingEnabled(bool value);
    int unreadCount() const { return m_unreadItems.size(); }
    QnNotificationLevel::Value unreadImportance() const { return m_unreadImportance; }
    auto makeUnreadCountGuard();

    core::VisibleItemDataDecoratorModel* visibleItemDataModel() const;
    core::AbstractSearchListModel* searchModel();
    const core::AbstractSearchListModel* searchModel() const;

    void setHighlightedTimestamp(std::chrono::microseconds value);
    void setHighlightedResources(const QSet<QnResourcePtr>& value);
    bool isHighlighted(const QModelIndex& index) const;

    void ensureAnalyticsRowVisible(int row);

    analytics::taxonomy::AttributeDisplayManager* attributeManager() const;
    void setAttributeManager(analytics::taxonomy::AttributeDisplayManager* value);

    static QString valuesText(const QStringList& values);

    static void calculatePreviewRects(const QnResourcePtr& resource,
        const QRectF& highlightRect,
        QRectF& cropRect,
        QRectF& newHighlightRect);

    enum Role
    {
        PreviewIdRole = Qn::ItemDataRoleCount,
        CommandActionRole,
        AdditionalActionRole,
        FlatFilteredAttributeListRole
    };

private:
    void recreateSourceModel();
    int calculateItemCount() const;
    void updateItemCount();
    void updateIsConstrained();
    void updateIsPlaceholderRequired();
    void updateSynchronizer();
    void createDeadlines(int first, int count);
    void closeExpired();
    void updateHighlight();
    void createUnread(int first, int count);
    void allItemsDataChangeNotify(const QList<int>& roles);

private:
    CommonObjectSearchSetup* const m_commonSetup = new CommonObjectSearchSetup(this);

    WindowContext* m_context = nullptr;
    core::EventSearch::SearchType m_type = core::EventSearch::SearchType::invalid;

    bool m_active = false;

    nx::utils::ScopedConnections m_contextConnections;
    nx::utils::ScopedConnections m_modelConnections;

    std::unique_ptr<AbstractSearchSynchronizer> m_synchronizer;

    bool m_constrained = false;
    bool m_isPlaceholderRequired = false;
    int m_itemCount = 0;

    struct Deadline
    {
        QDeadlineTimer timer;
        bool paused = false;
    };

    QHash<QPersistentModelIndex, Deadline> m_deadlines;
    QTimer m_autoCloseTimer;

    core::OptionalFetchRequest m_fetchRequest;
    nx::utils::PendingOperation m_fetchOperation;

    std::unique_ptr<core::AnalyticsSearchSetup> m_analyticsSetup;
    QHash<QPersistentModelIndex, QnNotificationLevel::Value> m_unreadItems;
    QnNotificationLevel::Value m_unreadImportance = QnNotificationLevel::Value::NoNotification;
    bool m_unreadTrackingEnabled = false;

    DetectableObjectTypeModel* m_objectTypeModel = nullptr; //< Owned by `m_analyticsSetup`.

    std::unique_ptr<AnalyticsEventModel> m_analyticsEvents;
    analytics::taxonomy::AttributeDisplayManager* m_attributeManager = nullptr;

    microseconds m_highlightedTimestamp = -1us;
    QSet<QnResourcePtr> m_highlightedResources;
};

// ------------------------------------------------------------------------------------------------
// RightPanelModelsAdapter

RightPanelModelsAdapter::RightPanelModelsAdapter(QObject* parent) :
    base_type(parent),
    d(new Private(this))
{
}

RightPanelModelsAdapter::~RightPanelModelsAdapter()
{
    // Required here for forward-declared pointer destruction.
}

WindowContext* RightPanelModelsAdapter::context() const
{
    return d->context();
}

void RightPanelModelsAdapter::setContext(WindowContext* value)
{
    d->setContext(value);
}

core::EventSearch::SearchType RightPanelModelsAdapter::type() const
{
    return d->type();
}

void RightPanelModelsAdapter::setType(core::EventSearch::SearchType value)
{
    d->setType(value);
}

bool RightPanelModelsAdapter::isOnline() const
{
    if (auto model = d->searchModel())
        return model->isOnline();

    return false;
}

bool RightPanelModelsAdapter::isAllowed() const
{
    return d->isAllowed();
}

bool RightPanelModelsAdapter::active() const
{
    return d->active();
}

void RightPanelModelsAdapter::setActive(bool value)
{
    d->setActive(value);
}

CommonObjectSearchSetup* RightPanelModelsAdapter::commonSetup() const
{
    return d->commonSetup();
}

core::AnalyticsSearchSetup* RightPanelModelsAdapter::analyticsSetup() const
{
    return d->analyticsSetup();
}

DetectableObjectTypeModel* RightPanelModelsAdapter::objectTypeModel() const
{
    return d->objectTypeModel();
}

QVariant RightPanelModelsAdapter::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(checkIndex(index)))
        return {};

    switch (role)
    {
        case core::ResourceRole:
        {
            const auto resource = base_type::data(index, role).value<QnResourcePtr>();
            if (!resource)
                return {};

            return QVariant::fromValue(core::withCppOwnership(resource.get()));
        }

        case core::DisplayedResourceListRole:
        {
            const auto data = base_type::data(index, role);
            QStringList result = data.value<QStringList>();

            if (result.empty())
            {
                auto resources = data.value<QnResourceList>();
                for (const auto& resource: resources)
                    result.push_back(common::html::bold(resource->getName()));
            }

            return result;
        }

        case core::IsHighlightedRole:
            return d->isHighlighted(index);

        case Qt::DecorationRole:
        case core::DecorationPathRole:
        {
            switch (d->type())
            {
                case core::EventSearch::SearchType::events:
                case core::EventSearch::SearchType::analytics:
                    return base_type::data(index, role);

                default:
                    return {};
            }
        }

        case Private::FlatFilteredAttributeListRole:
        {
            const auto attributes = data(index, core::AnalyticsAttributesRole)
                .value<core::analytics::AttributeList>();
            return AnalyticsSearchListModel::flattenAttributeList(
                filterAndSortAttributeList(attributes,
                    d->attributeManager()->attributes(),
                    d->attributeManager()->visibleAttributes()));
        }

        case Qn::ProgressValueRole:
        {
            const auto progressVariant = base_type::data(index, role);
            if (!progressVariant.canConvert<ProgressState>())
                return {};

            const auto progress = progressVariant.value<ProgressState>();

            return progress.value() ? *progress.value() : QVariant{};
        }

        case Private::CommandActionRole:
        {
            return CommandAction::createQmlAction(
                data(index, Qn::CommandActionRole).value<CommandActionPtr>(),
                core::appContext()->qmlEngine()).toVariant();
        }

        case Private::AdditionalActionRole:
        {
            return CommandAction::createQmlAction(
                data(index, Qn::AdditionalActionRole).value<CommandActionPtr>(),
                core::appContext()->qmlEngine()).toVariant();
        }

        default:
            return base_type::data(index, role);
    }
}

QHash<int, QByteArray> RightPanelModelsAdapter::roleNames() const
{
    auto roles = base_type::roleNames();
    roles.insert(core::clientCoreRoleNames());

    roles[Qt::ForegroundRole] = "foregroundColor";
    roles[Qn::AdditionalTextRole] = "additionalText";
    roles[Qn::RemovableRole] = "isCloseable";
    roles[Qn::AlternateColorRole] = "isInformer";
    roles[Qn::ProgressValueRole] = "progressValue";
    roles[Qn::HelpTopicIdRole] = "helpTopicId";
    roles[Private::FlatFilteredAttributeListRole] = "filteredAttributes";
    return roles;
}

bool RightPanelModelsAdapter::removeRow(int row)
{
    return base_type::removeRows(row, 1);
}

void RightPanelModelsAdapter::setAutoClosePaused(int row, bool value)
{
    d->setAutoClosePaused(row, value);
}

void RightPanelModelsAdapter::setLivePaused(bool value)
{
    if (auto model = d->searchModel())
        model->setLivePaused(value);
}

void RightPanelModelsAdapter::click(int row, int button, int modifiers)
{
    emit clicked(index(row, 0), Qt::MouseButton(button), Qt::KeyboardModifiers(modifiers));
}

void RightPanelModelsAdapter::doubleClick(int row)
{
    emit doubleClicked(index(row, 0));
}

void RightPanelModelsAdapter::startDrag(int row, const QPoint& pos, const QSize& size)
{
    emit dragStarted(index(row, 0), pos, size);
}

void RightPanelModelsAdapter::activateLink(int row, const QString& link)
{
    if (link.contains("://"))
        QDesktopServices::openUrl(link);
    else
        emit linkActivated(index(row, 0), link);
}

void RightPanelModelsAdapter::showContextMenu(int row, const QPoint& globalPos,
    bool withStandardInteraction = true)
{
    emit contextMenuRequested(index(row, 0), globalPos, withStandardInteraction,
        context()->mainWindowWidget());
}

void RightPanelModelsAdapter::addCameraToLayout()
{
    if (!NX_ASSERT(d->context()))
        return;

    struct Policy: public CameraSelectionDialog::DummyPolicy
    {
        using resource_type = QnVirtualCameraResource;
        static bool emptyListIsValid() { return false; }
        static bool multiChoiceListIsValid() { return false; }
    };

    UuidSet chosenIds;
    if (!CameraSelectionDialog::selectCameras<Policy>(chosenIds, d->context()->mainWindowWidget()))
        return;

    const auto cameras = d->context()->system()->resourcePool()
        ->getResourcesByIds<QnVirtualCameraResource>(chosenIds);

    if (!cameras.empty())
        d->context()->menu()->trigger(menu::DropResourcesAction, cameras);
}

void RightPanelModelsAdapter::showOnLayout(int row)
{
    if (!NX_ASSERT(row >= 0))
        return;

    d->context()->menu()->action(menu::ObjectsTabAction)->trigger();

    // Bring main window to front, event from minimized state.
    const auto mainWindow = d->context()->mainWindowWidget();
    mainWindow->setVisible(true);
    mainWindow->setWindowState(
        (mainWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    mainWindow->raise();
    mainWindow->activateWindow();

    if (type() == core::EventSearch::SearchType::analytics)
        d->ensureAnalyticsRowVisible(row);

    executeLater(
        [this, index = QPersistentModelIndex(index(row, 0))]()
        {
            if (!index.isValid())
                return;
            click(index.row(), Qt::LeftButton, Qt::NoModifier);
            doubleClick(index.row());
        },
        this);
}

void RightPanelModelsAdapter::showEventLog()
{
    if (NX_ASSERT(d->context()))
        d->context()->menu()->trigger(menu::OpenBusinessLogAction);
}

void RightPanelModelsAdapter::setHighlightedTimestamp(microseconds value)
{
    d->setHighlightedTimestamp(value);
}

void RightPanelModelsAdapter::setHighlightedResources(const QSet<QnResourcePtr>& value)
{
    d->setHighlightedResources(value);
}

core::FetchRequest RightPanelModelsAdapter::requestForDirection(
    core::EventSearch::FetchDirection direction)
{
    if (const auto model = d->searchModel())
        return model->requestForDirection(direction);

    NX_ASSERT(false, "Maodel isn't set");
    return {};
}

int RightPanelModelsAdapter::itemCount() const
{
    return d->itemCount();
}

QString RightPanelModelsAdapter::itemCountText() const
{
    const int count = itemCount();
    if (count == 0)
        return QString();

    const auto formatItemCountText =
        [this](int count)
        {
            switch (type())
            {
                case core::EventSearch::SearchType::notifications:
                    return tr("%n notifications", "", count);

                case core::EventSearch::SearchType::motion:
                    return tr("%n motion events", "", count);

                case core::EventSearch::SearchType::bookmarks:
                    return tr("%n bookmarks", "", count);

                case core::EventSearch::SearchType::events:
                    return tr("%n events", "", count);

                case core::EventSearch::SearchType::analytics:
                    return tr("%n objects", "", count);

                default:
                    return QString();
            }
        };

    static constexpr int kThreshold = 99;
    return count > kThreshold && !kDebugShowFullItemCount
        ? QString(">") + formatItemCountText(kThreshold)
        : formatItemCountText(count);
}

bool RightPanelModelsAdapter::isConstrained() const
{
    return d->isConstrained();
}

bool RightPanelModelsAdapter::isPlaceholderRequired() const
{
    return d->isPlaceholderRequired();
}

bool RightPanelModelsAdapter::unreadTrackingEnabled() const
{
    return d->unreadTrackingEnabled();
}

void RightPanelModelsAdapter::setUnreadTrackingEnabled(bool value)
{
    d->setUnreadTrackingEnabled(value);
}

int RightPanelModelsAdapter::unreadCount() const
{
    return d->unreadCount();
}

QColor RightPanelModelsAdapter::unreadColor() const
{
    return QnNotificationLevel::notificationTextColor(d->unreadImportance());
}

bool RightPanelModelsAdapter::previewsEnabled() const
{
    if (const auto model = d->visibleItemDataModel())
        return model->previewsEnabled();

    return false;
}

void RightPanelModelsAdapter::setPreviewsEnabled(bool value)
{
    if (const auto model = d->visibleItemDataModel())
        return model->setPreviewsEnabled(value);
}

void RightPanelModelsAdapter::fetchData(const core::FetchRequest& request,
    bool immediately)
{
    d->fetchData(request, immediately);
}

bool RightPanelModelsAdapter::fetchInProgress() const
{
    return d->fetchInProgress();
}

QVector<RightPanel::VmsEventGroup> RightPanelModelsAdapter::eventGroups() const
{
    if (!d->context())
        return {};

    const auto systemContext = d->context()->system();
    nx::vms::event::StringsHelper helper(systemContext);

    QVector<RightPanel::VmsEventGroup> result{
        RightPanel::VmsEventGroup{RightPanel::VmsEventGroup::Common,
            nx::vms::api::undefinedEvent,
            "", //< Name is unused.
            rules::Strings::anyEvent()
        },
        RightPanel::VmsEventGroup{RightPanel::VmsEventGroup::DeviceIssues,
            nx::vms::api::anyCameraEvent,
            rules::Strings::deviceIssues(systemContext),
            rules::Strings::anyDeviceIssue(systemContext)},
        RightPanel::VmsEventGroup{RightPanel::VmsEventGroup::Server,
            nx::vms::api::anyServerEvent,
            rules::Strings::serverEvents(),
            rules::Strings::anyServerEvent()},
        RightPanel::VmsEventGroup{RightPanel::VmsEventGroup::Analytics,
            nx::vms::api::analyticsSdkEvent,
            rules::Strings::analyticsEvents(),
            rules::Strings::anyAnalyticsEvent()}};

    for (auto& group: result)
    {
        const auto parentId = group.id == nx::vms::api::undefinedEvent
            ? nx::vms::api::anyEvent
            : group.id;

        for (const auto eventType: nx::vms::event::allEvents())
        {
            if (eventType != nx::vms::api::analyticsSdkEvent
                && nx::vms::event::parentEvent(eventType) == parentId)
            {
                group.events.push_back({eventType, helper.eventName(eventType)});
            }
        }
    }

    return result;
}

QAbstractItemModel* RightPanelModelsAdapter::analyticsEvents() const
{
    return d->analyticsEvents();
}

analytics::taxonomy::AttributeDisplayManager* RightPanelModelsAdapter::attributeManager() const
{
    return d->attributeManager();
}

void RightPanelModelsAdapter::setAttributeManager(
    analytics::taxonomy::AttributeDisplayManager* attributeManager)
{
    d->setAttributeManager(attributeManager);
}

void RightPanelModelsAdapter::registerQmlTypes()
{
    qmlRegisterType<RightPanelModelsAdapter>("nx.vms.client.desktop", 1, 0, "RightPanelModel");

    qmlRegisterUncreatableType<CommonObjectSearchSetup>("nx.vms.client.desktop", 1, 0,
        "CommonObjectSearchSetup", "Cannot create instance of CommonObjectSearchSetup");

    qmlRegisterUncreatableType<DetectableObjectTypeModel>("nx.vms.client.desktop.analytics", 1, 0,
        "ObjectTypeModel", "Cannot create instance of ObjectTypeModel");
}

// ------------------------------------------------------------------------------------------------
// RightPanelModelsAdapter::Private

auto RightPanelModelsAdapter::Private::makeUnreadCountGuard()
{
    return nx::utils::makeScopeGuard(
        [this, oldCount = m_unreadItems.size()]()
        {
            if (oldCount == m_unreadItems.size())
                return;

            m_unreadImportance = QnNotificationLevel::Value::NoNotification;
            for (const auto level: m_unreadItems)
            {
                if (level > m_unreadImportance)
                    m_unreadImportance = level;

                if (m_unreadImportance == QnNotificationLevel::Value::CriticalNotification)
                    break;
            }

            emit q->unreadCountChanged();
        });
}

RightPanelModelsAdapter::Private::Private(RightPanelModelsAdapter* q):
    q(q)
{
    connect(q, &QAbstractListModel::modelAboutToBeReset, this,
        [this]
        {
            m_deadlines.clear();

            const auto guard = makeUnreadCountGuard();
            m_unreadItems.clear();
        });

    connect(q, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int row = first; row <= last; ++row)
                m_deadlines.remove(this->q->index(row, 0));

            const auto guard = makeUnreadCountGuard();
            m_unreadItems.removeIf(
                [first, last](
                    const QHash<QPersistentModelIndex, QnNotificationLevel::Value>::iterator it)
                {
                    return it.key().row() >= first && it.key().row() <= last;
                });
        });

    connect(q, &QAbstractListModel::modelReset, this,
        [this]
        {
            const auto count = this->q->rowCount();
            createDeadlines(0, count);
        });

    connect(q, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            if (!NX_ASSERT(!parent.isValid()))
                return;

            const auto count = last - first + 1;
            createDeadlines(first, count);
            createUnread(first, count);
        });

    m_autoCloseTimer.callOnTimeout(this, &Private::closeExpired);
    m_autoCloseTimer.start(1s);

    m_fetchOperation.setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_fetchOperation.setInterval(kQueuedFetchDelay);
    m_fetchOperation.setCallback(
        [this]()
        {
            if (!NX_ASSERT(m_fetchRequest))
                return;

            if (const auto model = searchModel();
                model && model->canFetchData(m_fetchRequest->direction))
            {
                model->fetchData(*m_fetchRequest);
                m_fetchRequest = {};
            }
        });

    connect(q, &RightPanelModelsAdapter::fetchFinished,
        this, &Private::updateIsPlaceholderRequired);

    connect(q, &RightPanelModelsAdapter::isOnlineChanged, q,
        [q]()
        {
            emit q->allowanceChanged();
            emit q->eventGroupsChanged();
        });
}

void RightPanelModelsAdapter::Private::setContext(WindowContext* value)
{
    if (m_context == value)
        return;

    m_contextConnections.reset();

    m_context = value;
    m_commonSetup->setContext(m_context);

    if (m_context)
    {
        TileInteractionHandler::install(q);

        m_contextConnections << connect(
            m_context->workbenchContext()->accessController(),
            &core::AccessController::permissionsMaybeChanged,
            q,
            &RightPanelModelsAdapter::allowanceChanged);
    }

    recreateSourceModel();
    emit q->contextChanged();
    emit q->allowanceChanged();
    emit q->eventGroupsChanged();
}

void RightPanelModelsAdapter::Private::setType(core::EventSearch::SearchType value)
{
    if (m_type == value)
        return;

    m_type = value;
    recreateSourceModel();
    emit q->typeChanged();
    emit q->allowanceChanged();
}

bool RightPanelModelsAdapter::Private::isAllowed() const
{
    using namespace nx::analytics::taxonomy;

    if (!m_context)
        return false;

    const auto accessController = m_context->workbenchContext()->accessController();
    switch (m_type)
    {
        case core::EventSearch::SearchType::motion:
        {
            // Prohibited for live viewers but should be allowed when browsing local files offline.
            return !q->isOnline() || accessController->isDeviceAccessRelevant(
                nx::vms::api::AccessRight::viewArchive);
        }

        case core::EventSearch::SearchType::bookmarks:
        {
            return q->isOnline() && accessController->isDeviceAccessRelevant(
                nx::vms::api::AccessRight::viewBookmarks);
        }

        case core::EventSearch::SearchType::events:
        {
            return q->isOnline()
                && accessController->hasGlobalPermissions(GlobalPermission::viewLogs);
        }

        case core::EventSearch::SearchType::analytics:
        {
            const bool hasPermissions = q->isOnline() && accessController->isDeviceAccessRelevant(
                nx::vms::api::AccessRight::viewArchive);

            if (!hasPermissions)
                return false;

            std::vector<AbstractObjectType*> objectTypes;
            const auto state = m_context->system()->analyticsTaxonomyState();
            if (state)
                objectTypes = state->rootObjectTypes();

            return std::any_of(objectTypes.cbegin(), objectTypes.cend(),
                [](nx::analytics::taxonomy::AbstractObjectType* objectType)
                {
                    return objectType->isReachable();
                });
        }

        case core::EventSearch::SearchType::notifications:
            return true;

        case core::EventSearch::SearchType::invalid:
            return false;
    }

    NX_ASSERT(false);
    return false;
}

bool RightPanelModelsAdapter::Private::active() const
{
    return m_active;
}

void RightPanelModelsAdapter::Private::setActive(bool value)
{
    if (m_active == value)
        return;

    m_active = value;
    emit q->activeChanged();

    if (m_synchronizer)
        m_synchronizer->setActive(m_active);
}

void RightPanelModelsAdapter::Private::recreateSourceModel()
{
    auto analyticsSetupGuard = nx::utils::makeScopeGuard(
        [this, hadAnalyticsSetup = (bool) m_analyticsSetup]()
        {
            if (hadAnalyticsSetup != (bool) m_analyticsSetup)
                emit q->analyticsSetupChanged();
        });

    auto isOnlineGuard = nx::utils::makeScopeGuard(
        [this, wasOnline = q->isOnline()]()
        {
            if (wasOnline != q->isOnline())
                emit q->isOnlineChanged();
        });

    auto analyticsEventsGuard = nx::utils::makeScopeGuard(
        [this]() { q->analyticsEventsChanged(); });

    const auto oldSourceModel = q->sourceModel();
    q->setSourceModel(nullptr);

    m_commonSetup->setModel(nullptr);
    m_synchronizer.reset();
    m_analyticsSetup.reset();
    m_modelConnections.reset();
    m_analyticsEvents.reset();

    if (oldSourceModel && oldSourceModel->parent() == this)
        delete oldSourceModel;

    if (!m_context)
        return;

    const auto settings = core::VisibleItemDataDecoratorModel::Settings {
        .tilePreviewLoadIntervalMs = std::chrono::milliseconds(ini().tilePreviewLoadIntervalMs),
        .previewReloadDelayMs = std::chrono::milliseconds(ini().rightPanelPreviewReloadDelay),
        .maxSimultaneousPreviewLoadsArm = ini().maxSimultaneousTilePreviewLoadsArm};

    const auto visualItemDecorator = new core::VisibleItemDataDecoratorModel(settings, this);
    switch (m_type)
    {
        case core::EventSearch::SearchType::invalid:
            break;

        case core::EventSearch::SearchType::notifications:
            visualItemDecorator->setSourceModel(
                new NotificationTabModel(m_context, visualItemDecorator));
            break;

        case core::EventSearch::SearchType::motion:
            visualItemDecorator->setSourceModel(
                new SimpleMotionSearchListModel(m_context, visualItemDecorator));
            break;

        case core::EventSearch::SearchType::bookmarks:
            visualItemDecorator->setSourceModel(
                new BookmarkSearchListModel(m_context->system(), visualItemDecorator));
            break;

        case core::EventSearch::SearchType::events:
        {
            const auto eventsModel = new EventSearchListModel(m_context, visualItemDecorator);
            visualItemDecorator->setSourceModel(eventsModel);

            using nx::vms::api::EventType;
            eventsModel->setDefaultEventTypes({
                EventType::analyticsSdkEvent,
                EventType::cameraDisconnectEvent,
                EventType::cameraInputEvent,
                EventType::cameraIpConflictEvent,
                EventType::networkIssueEvent,
                EventType::softwareTriggerEvent,
                EventType::userDefinedEvent});

            m_analyticsEvents.reset(
                new AnalyticsEventModel(
                    m_context->system()->analyticsEventsSearchTreeBuilder(),
                    q->sourceModel()));

            break;
        }

        case core::EventSearch::SearchType::analytics:
        {
            const auto analyticsModel = new AnalyticsSearchListModel(
                m_context->system(), visualItemDecorator);
            analyticsModel->setLiveProcessingMode(
                core::AnalyticsSearchListModel::LiveProcessingMode::manualAdd);
            m_analyticsSetup.reset(new core::AnalyticsSearchSetup(analyticsModel));
            visualItemDecorator->setSourceModel(analyticsModel);

            const auto analyticsFilterModel =
                m_context->system()->taxonomyManager()->createFilterModel(
                    /*parent*/ m_analyticsSetup.get());

            m_objectTypeModel = new DetectableObjectTypeModel(analyticsFilterModel,
                /*parent*/ m_analyticsSetup.get());

            m_modelConnections << connect(
                q->commonSetup(),
                &CommonObjectSearchSetup::selectedCamerasChanged,
                m_objectTypeModel,
                [this]
                {
                    m_objectTypeModel->sourceModel()->setSelectedDevices(
                        q->commonSetup()->selectedCameras());
                });

            m_modelConnections << connect(
                q->analyticsSetup(),
                &core::AnalyticsSearchSetup::engineChanged,
                m_objectTypeModel,
                [this]
                {
                    const auto engine = m_objectTypeModel->sourceModel()->findEngine(
                        q->analyticsSetup()->engine());

                    m_objectTypeModel->setEngine(engine);
                });

            m_modelConnections << connect(
                m_context->system()->analyticsTaxonomyStateWatcher(),
                qOverload<>(&nx::analytics::taxonomy::AbstractStateWatcher::stateChanged),
                q,
                &RightPanelModelsAdapter::allowanceChanged);

            m_modelConnections << connect(
                analyticsModel, &AnalyticsSearchListModel::pluginActionRequested,
                q, &RightPanelModelsAdapter::pluginActionRequested);

            break;
        }
    }

    q->setSourceModel(visualItemDecorator);
    if (auto model = q->sourceModel())
    {
        m_modelConnections << connect(model, &QAbstractItemModel::modelReset, this,
            [this]()
            {
                updateIsConstrained();
                updateItemCount();
            });

        m_modelConnections << connect(model, &QAbstractItemModel::rowsRemoved,
            this, &Private::updateItemCount);

        m_modelConnections << connect(model, &QAbstractItemModel::rowsInserted,
            this, &Private::updateItemCount);

        m_modelConnections << connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& topLeft,
                const QModelIndex& bottomRight,
                const QList<int>& roles = QList<int>())
            {
                if (!roles.contains(core::IsVisibleRole))
                    return;

                for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
                {
                    const auto index = q->index(row, 0);
                    if (!index.data(core::IsVisibleRole).toBool())
                        continue;

                    const auto iter = m_deadlines.find(index);
                    if (iter != m_deadlines.cend())
                        iter->timer.setRemainingTime(kVisibleAutoCloseDelay);
                }
            });
    }

    if (auto model = searchModel())
    {
        m_commonSetup->setModel(model);

        m_modelConnections << connect(model, &core::AbstractSearchListModel::dataNeeded, this,
            [this]()
            {
                updateIsConstrained();
                updateIsPlaceholderRequired();
                emit q->dataNeeded();
            });

        m_modelConnections << connect(model, &core::AbstractSearchListModel::fetchCommitStarted,
            q, &RightPanelModelsAdapter::fetchCommitStarted);

        m_modelConnections << connect(model, &core::AbstractSearchListModel::fetchFinished,
            q, &RightPanelModelsAdapter::fetchFinished);

        m_modelConnections << connect(model, &core::AbstractSearchListModel::isOnlineChanged,
            q, &RightPanelModelsAdapter::isOnlineChanged);

        if (auto asyncModel = qobject_cast<core::AbstractAsyncSearchListModel*>(model))
        {
            m_modelConnections
                << connect(asyncModel, &core::AbstractAsyncSearchListModel::asyncFetchStarted,
                    q, &RightPanelModelsAdapter::asyncFetchStarted);
        }
    }

    updateSynchronizer();

    analyticsSetupGuard.fire();
    isOnlineGuard.fire();

    updateIsConstrained();
    updateItemCount();
}

void RightPanelModelsAdapter::Private::setHighlightedTimestamp(microseconds value)
{
    if (m_highlightedTimestamp == value)
        return;

    m_highlightedTimestamp = value;
    updateHighlight();
}

void RightPanelModelsAdapter::Private::setHighlightedResources(const QSet<QnResourcePtr>& value)
{
    if (m_highlightedResources == value)
        return;

    m_highlightedResources = value;
    updateHighlight();
}

void RightPanelModelsAdapter::Private::updateHighlight()
{
    const auto count = q->rowCount();
    if (!count)
        return;

    emit q->dataChanged(q->index(0, 0), q->index(count - 1, 0), {core::IsHighlightedRole});
}

bool RightPanelModelsAdapter::Private::isHighlighted(const QModelIndex& index) const
{
    if (m_highlightedTimestamp <= 0ms || m_highlightedResources.empty())
        return false;

    const auto timestamp = index.data(core::TimestampRole).value<microseconds>();
    if (timestamp <= 0us)
        return false;

    const auto duration = index.data(core::DurationRole).value<microseconds>();
    if (duration <= 0us)
        return false;

    const auto isHighlightedResource =
        [this](const QnResourcePtr& res) { return m_highlightedResources.contains(res); };

    const auto resources = index.data(core::ResourceListRole).value<QnResourceList>();
    if (std::none_of(resources.cbegin(), resources.cend(), isHighlightedResource))
        return false;

    return m_highlightedTimestamp >= timestamp
        && (duration.count() == QnTimePeriod::kInfiniteDuration
            || m_highlightedTimestamp <= (timestamp + duration));
}

void RightPanelModelsAdapter::Private::updateSynchronizer()
{
    if (!NX_ASSERT(m_context))
        return;

    switch (m_type)
    {
        case core::EventSearch::SearchType::motion:
        {
            const auto model = qobject_cast<SimpleMotionSearchListModel*>(q->sourceModel());
            m_synchronizer.reset(new MotionSearchSynchronizer(m_context, m_commonSetup, model));
            break;
        }

        case core::EventSearch::SearchType::bookmarks:
        {
            m_synchronizer.reset(new BookmarkSearchSynchronizer(m_context, m_commonSetup));
            break;
        }

        case core::EventSearch::SearchType::analytics:
        {
            m_synchronizer.reset(
                new AnalyticsSearchSynchronizer(m_context, m_commonSetup, m_analyticsSetup.get()));
            break;
        }

        default:
        {
            m_synchronizer.reset();
            break;
        }
    }

    if (!m_synchronizer)
        return;

    connect(m_synchronizer.get(), &AbstractSearchSynchronizer::activeChanged,
        this, &Private::setActive);

    m_synchronizer->setActive(m_active);
}

void RightPanelModelsAdapter::Private::createDeadlines(int first, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const auto index = q->index(first + i, 0);
        const auto closeable = index.data(Qn::RemovableRole).toBool();
        const auto timeout = index.data(Qn::TimeoutRole).value<milliseconds>();

        if (closeable && timeout > 0ms)
            m_deadlines[index].timer = QDeadlineTimer(kInvisibleAutoCloseDelay);
    }
}

int RightPanelModelsAdapter::Private::calculateItemCount() const
{
    if (!q->sourceModel())
        return 0;

    if (const auto notificationsModel = qobject_cast<NotificationTabModel*>(q->sourceModel()))
        return notificationsModel->effectiveCount();

    return q->sourceModel()->rowCount();
}

void RightPanelModelsAdapter::Private::updateItemCount()
{
    const int value = calculateItemCount();
    if (value == m_itemCount)
        return;

    m_itemCount = value;
    emit q->itemCountChanged();

    updateIsPlaceholderRequired();
}

void RightPanelModelsAdapter::Private::updateIsPlaceholderRequired()
{
    const bool canFetch =
        [this]()
        {
            const auto model = searchModel();
            return model && (model->canFetchData(core::EventSearch::FetchDirection::newer)
                || model->canFetchData(core::EventSearch::FetchDirection::older));
        }();
    const bool placeholderRequired = m_itemCount == 0 && !canFetch;
    if (m_isPlaceholderRequired == placeholderRequired)
        return;

    m_isPlaceholderRequired = placeholderRequired;
    emit q->placeholderRequiredChanged();
}

void RightPanelModelsAdapter::Private::updateIsConstrained()
{
    const auto model = searchModel();
    const bool value = model && model->isConstrained();

    if (value == m_constrained)
        return;

    m_constrained = value;
    emit q->isConstrainedChanged();
}

void RightPanelModelsAdapter::Private::closeExpired()
{
    QList<QPersistentModelIndex> expired;
    const int oldDeadlineCount = m_deadlines.size();

    for (const auto& [index, deadline]: nx::utils::keyValueRange(m_deadlines))
    {
        if (!deadline.paused && deadline.timer.hasExpired() && index.isValid())
            expired.push_back(index);
    }

    if (expired.empty())
        return;

    for (const auto& index: expired)
        q->removeRow(index.row());

    NX_VERBOSE(q, "Expired %1 tiles", expired.size());
    NX_ASSERT(expired.size() == (oldDeadlineCount - m_deadlines.size()));
}

void RightPanelModelsAdapter::Private::createUnread(int first, int count)
{
    if (!m_unreadTrackingEnabled)
        return;

    const auto visibleModel = visibleItemDataModel();
    if (!NX_ASSERT(visibleModel))
        return;

    const auto guard = makeUnreadCountGuard();
    for (int i = 0; i != count; ++i)
    {
        const auto index = q->index(first + i, 0);
        if (visibleModel->isVisible(index))
            continue;

        const auto level = index.data(Qn::NotificationLevelRole).value<QnNotificationLevel::Value>();
        m_unreadItems.insert(index, level);
    }
}

void RightPanelModelsAdapter::Private::setAutoClosePaused(int row, bool value)
{
    const auto iter = m_deadlines.find(q->index(row, 0));
    if (iter != m_deadlines.cend())
    {
        iter->paused = value;
        if (!value)
            iter->timer.setRemainingTime(kVisibleAutoCloseDelay);
    }
}

void RightPanelModelsAdapter::Private::allItemsDataChangeNotify(const QList<int>& roles)
{
    emit q->dataChanged(
        q->index(0, 0),
        q->index(q->rowCount() - 1, q->columnCount() - 1),
        roles);
}

void RightPanelModelsAdapter::Private::fetchData(const core::FetchRequest& request,
    bool immediately)
{
    m_fetchRequest = request;
    if (immediately)
        m_fetchOperation.fire();
    else
        m_fetchOperation.requestOperation();
}

bool RightPanelModelsAdapter::Private::fetchInProgress() const
{
    const auto model = searchModel();
    return model && model->fetchInProgress();
}

core::VisibleItemDataDecoratorModel* RightPanelModelsAdapter::Private::visibleItemDataModel() const
{
    return qobject_cast<core::VisibleItemDataDecoratorModel*>(q->sourceModel());
}

core::AbstractSearchListModel* RightPanelModelsAdapter::Private::searchModel()
{
    const auto model = visibleItemDataModel();
    return model
        ? qobject_cast<core::AbstractSearchListModel*>(model->sourceModel())
        : nullptr;
}

const core::AbstractSearchListModel* RightPanelModelsAdapter::Private::searchModel() const
{
    const auto model = visibleItemDataModel();
    return model
        ? qobject_cast<const core::AbstractSearchListModel*>(model->sourceModel())
        : nullptr;
}

void RightPanelModelsAdapter::Private::ensureAnalyticsRowVisible(int row)
{
    const auto synchronizer = qobject_cast<AnalyticsSearchSynchronizer*>(m_synchronizer.get());
    const auto index = q->index(row, 0);
    if (!NX_ASSERT(synchronizer && m_analyticsSetup && index.isValid()))
        return;

    const auto timestamp = index.data(core::TimestampRole).value<microseconds>();
    const auto trackId = index.data(core::ObjectTrackIdRole).value<nx::Uuid>();
    if (const auto& timeWindow = m_analyticsSetup->model()->fetchedTimeWindow())
        synchronizer->ensureVisible(duration_cast<milliseconds>(timestamp), trackId, *timeWindow);
}

void RightPanelModelsAdapter::Private::setUnreadTrackingEnabled(bool value)
{
    if (m_unreadTrackingEnabled == value)
        return;

    m_unreadTrackingEnabled = value;
    emit q->unreadTrackingChanged();

    if (!m_unreadTrackingEnabled)
        return;

    const auto guard = makeUnreadCountGuard();
    m_unreadItems.clear();
}

analytics::taxonomy::AttributeDisplayManager*
    RightPanelModelsAdapter::Private::attributeManager() const
{
    return m_attributeManager;
}

void RightPanelModelsAdapter::Private::setAttributeManager(
    analytics::taxonomy::AttributeDisplayManager* value)
{
    using namespace analytics::taxonomy;

    if (value == m_attributeManager)
        return;

    if (m_attributeManager)
        m_attributeManager->disconnect(this);

    m_attributeManager = value;

    if (m_attributeManager)
    {
        connect(m_attributeManager, &AttributeDisplayManager::attributesChanged, this,
            [this]() { allItemsDataChangeNotify({FlatFilteredAttributeListRole}); });
        connect(m_attributeManager, &AttributeDisplayManager::visibleAttributesChanged, this,
            [this]() { allItemsDataChangeNotify({FlatFilteredAttributeListRole}); });
    }

    emit q->attributeManagerChanged();
}

} // namespace nx::vms::client::desktop

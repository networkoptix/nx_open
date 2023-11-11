// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "right_panel_models_adapter.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <limits>
#include <memory>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtQml/QtQml>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
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
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/thumbnails/abstract_caching_resource_thumbnail.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/common/utils/preview_rect_calculator.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/synchronizers/analytics_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/bookmark_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/synchronizers/motion_search_synchronizer.h>
#include <nx/vms/client/desktop/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/desktop/event_search/widgets/private/tile_interaction_handler_p.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
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

static constexpr int kMaxDisplayedGroupValues = 2; //< Before "(+n values)".

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

QnMediaServerResourcePtr previewServer(const ResourceThumbnailProvider* provider)
{
    const auto resource = provider->resource();
    return resource
        ? resource->getParentResource().dynamicCast<QnMediaServerResource>()
        : QnMediaServerResourcePtr();
}

bool isBusy(Qn::ThumbnailStatus status)
{
    return status == Qn::ThumbnailStatus::Loading || status == Qn::ThumbnailStatus::Refreshing;
}

qlonglong providerId(ResourceThumbnailProvider* provider)
{
    return qlonglong(provider);
}

QHash<intptr_t, QPointer<ResourceThumbnailProvider>>& previewsById()
{
    static QHash<intptr_t, QPointer<ResourceThumbnailProvider>> data;
    return data;
}

} // namespace

class RightPanelModelsAdapter::Private: public QObject
{
    RightPanelModelsAdapter* const q;

public:
    Private(RightPanelModelsAdapter* q);

    WindowContext* context() const { return m_context; }
    void setContext(WindowContext* value);

    Type type() const { return m_type; }
    void setType(Type value);

    bool isAllowed() const;

    bool active() const;
    void setActive(bool value);

    CommonObjectSearchSetup* commonSetup() const { return m_commonSetup; }
    AnalyticsSearchSetup* analyticsSetup() const { return m_analyticsSetup.get(); }
    QAbstractItemModel* analyticsEvents() const { return m_analyticsEvents.get(); }

    void setAutoClosePaused(int row, bool value);

    void requestFetch(bool immediately);
    bool fetchInProgress() const;
    bool canFetch() const;

    QString previewId(int row) const;
    qreal previewAspectRatio(int row) const;
    QRectF previewHighlightRect(int row) const;
    RightPanel::PreviewState previewState(int row) const;

    bool isVisible(const QModelIndex& index) const;
    bool setVisible(const QModelIndex& index, bool value);

    bool isConstrained() const { return m_constrained; }
    bool isPlaceholderRequired() const { return m_isPlaceholderRequired; }
    int itemCount() const { return m_itemCount; }

    AbstractSearchListModel* searchModel();
    const AbstractSearchListModel* searchModel() const;

    bool previewsEnabled() const { return m_previewsEnabled; }
    void setPreviewsEnabled(bool value);

    void setHighlightedTimestamp(std::chrono::microseconds value);
    void setHighlightedResources(const QSet<QnResourcePtr>& value);
    bool isHighlighted(const QModelIndex& index) const;

    void ensureAnalyticsRowVisible(int row);

    static QString valuesText(const QStringList& values);

    static void calculatePreviewRects(const QnResourcePtr& resource,
        const QRectF& highlightRect,
        QRectF& cropRect,
        QRectF& newHighlightRect);

    enum Role
    {
        PreviewIdRole = Qn::ItemDataRoleCount,
        PreviewStateRole,
        PreviewAspectRatioRole,
        PreviewTimeMsRole,
        PreviewHighlightRectRole,
        FlatAttributeListRole,
        ItemIsVisibleRole,
        HighlightedRole
    };

private:
    void recreateSourceModel();
    void updateItemCount();
    void updateIsConstrained();
    void updateIsPlaceholderRequired();
    void updateSynchronizer();
    void createDeadlines(int first, int count);
    void closeExpired();
    void updateHighlight();
    void createPreviews(int first, int count);
    void updatePreviewProvider(int row);
    void handlePreviewLoadingEnded(ResourceThumbnailProvider* provider);
    void requestPreview(int row);
    void loadNextPreview();
    bool isNextPreviewLoadAllowed(const ResourceThumbnailProvider* provider) const;

private:
    CommonObjectSearchSetup* const m_commonSetup = new CommonObjectSearchSetup(this);

    WindowContext* m_context = nullptr;
    Type m_type = Type::invalid;

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

    QTimer m_hangedPreviewRequestsChecker;
    bool m_previewsEnabled = true;

    nx::utils::PendingOperation m_fetchOperation;
    nx::utils::PendingOperation m_previewLoadOperation;
    nx::utils::ElapsedTimer m_sinceLastPreviewRequest;

    QSet<QPersistentModelIndex> m_visibleItems;

    std::unique_ptr<AnalyticsSearchSetup> m_analyticsSetup;

    std::unique_ptr<AnalyticsEventModel> m_analyticsEvents;

    microseconds m_highlightedTimestamp = -1us;
    QSet<QnResourcePtr> m_highlightedResources;

    // Previews.
    using PreviewProviderPtr = std::unique_ptr<ResourceThumbnailProvider>;
    struct PreviewData
    {
        PreviewProviderPtr provider;
        QDeadlineTimer reloadTimer;
        QRectF highlightRect;
    };

    std::deque<PreviewData> m_previews;

    enum class WaitingStatus
    {
        idle,
        requiresLoading,
        awaitsReloading
    };
    WaitingStatus waitingStatus(const PreviewData& data) const;

    struct PreviewLoadData
    {
        QnMediaServerResourcePtr server;
        QDeadlineTimer timeout;
    };

    struct ServerLoadData
    {
        int loadingCounter = 0;
    };

    QHash<ResourceThumbnailProvider*, PreviewLoadData> m_loadingByProvider;
    QHash<QnMediaServerResourcePtr, ServerLoadData> m_loadingByServer;
    ResourceThumbnailProvider* m_providerLoadingFromCache = nullptr;
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

RightPanelModelsAdapter::Type RightPanelModelsAdapter::type() const
{
    return d->type();
}

void RightPanelModelsAdapter::setType(Type value)
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

AnalyticsSearchSetup* RightPanelModelsAdapter::analyticsSetup() const
{
    return d->analyticsSetup();
}

QVariant RightPanelModelsAdapter::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(checkIndex(index)))
        return {};

    switch (role)
    {
        case Qn::ResourceRole:
        {
            const auto resource = base_type::data(index, role).value<QnResourcePtr>();
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        case Qn::DisplayedResourceListRole:
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

        case Private::FlatAttributeListRole:
        {
            const auto attributes = data(index, Qn::AnalyticsAttributesRole)
                .value<analytics::AttributeList>();

            return flattenAttributeList(attributes);
        }

        case Private::ItemIsVisibleRole:
            return d->isVisible(index);

        case Private::HighlightedRole:
            return d->isHighlighted(index);

        case Private::PreviewIdRole:
            return d->previewId(index.row());

        case Private::PreviewStateRole:
            return QVariant::fromValue(d->previewState(index.row()));

        case Private::PreviewAspectRatioRole:
            return d->previewAspectRatio(index.row());

        case Private::PreviewHighlightRectRole:
            return d->previewHighlightRect(index.row());

        case Private::PreviewTimeMsRole:
        {
            const auto time = data(index, Qn::PreviewTimeRole).value<microseconds>();
            return QVariant::fromValue(duration_cast<milliseconds>(time).count());
        }

        case Qt::DecorationRole:
        case Qn::DecorationPathRole:
        {
            switch (d->type())
            {
                case Type::events:
                case Type::analytics:
                    return base_type::data(index, role);

                default:
                    return {};
            }
        }

        case Qn::ProgressValueRole:
        {
            const auto progressVariant = base_type::data(index, role);
            if (!progressVariant.canConvert<ProgressState>())
                return {};

            const auto progress
                = progressVariant.value<ProgressState>();

            return progress.value() ? *progress.value() : QVariant{};
        }

        default:
            return base_type::data(index, role);
    }
}

bool RightPanelModelsAdapter::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Private::ItemIsVisibleRole || !index.isValid() || !NX_ASSERT(checkIndex(index)))
        return false;

    return d->setVisible(index, value.toBool());
}

QHash<int, QByteArray> RightPanelModelsAdapter::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[Qt::ForegroundRole] = "foregroundColor";
    roles[Qn::ResourceRole] = "previewResource";
    roles[Qn::DescriptionTextRole] = "description";
    roles[Qn::DecorationPathRole] = "decorationPath";
    roles[Qn::TimestampTextRole] = "timestamp";
    roles[Qn::AdditionalTextRole] = "additionalText";
    roles[Qn::DisplayedResourceListRole] = "resourceList";
    roles[Qn::RemovableRole] = "isCloseable";
    roles[Qn::AlternateColorRole] = "isInformer";
    roles[Qn::ProgressValueRole] = "progressValue";
    roles[Qn::HelpTopicIdRole] = "helpTopicId";
    roles[Qn::AnalyticsEngineNameRole] = "analyticsEngineName";
    roles[Qn::AnalyticsAttributesRole] = "rawAttributes";
    roles[Private::FlatAttributeListRole] = "attributes";
    roles[Private::ItemIsVisibleRole] = "visible";
    roles[Private::HighlightedRole] = "highlighted";
    roles[Private::PreviewIdRole] = "previewId";
    roles[Private::PreviewStateRole] = "previewState";
    roles[Private::PreviewAspectRatioRole] = "previewAspectRatio";
    roles[Private::PreviewTimeMsRole] = "previewTimestampMs";
    roles[Private::PreviewHighlightRectRole] = "previewHighlightRect";
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

void RightPanelModelsAdapter::setFetchDirection(RightPanel::FetchDirection value)
{
    if (auto model = d->searchModel())
        model->setFetchDirection(value);
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

    QnUuidSet chosenIds;
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

    if (type() == Type::analytics)
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
                case RightPanelModelsAdapter::Type::motion:
                    return tr("%n motion events", "", count);

                case RightPanelModelsAdapter::Type::bookmarks:
                    return tr("%n bookmarks", "", count);

                case RightPanelModelsAdapter::Type::events:
                    return tr("%n events", "", count);

                case RightPanelModelsAdapter::Type::analytics:
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

bool RightPanelModelsAdapter::canFetch() const
{
    return d->canFetch();
}

bool RightPanelModelsAdapter::isConstrained() const
{
    return d->isConstrained();
}

bool RightPanelModelsAdapter::isPlaceholderRequired() const
{
    return d->isPlaceholderRequired();
}

bool RightPanelModelsAdapter::previewsEnabled() const
{
    return d->previewsEnabled();
}

void RightPanelModelsAdapter::setPreviewsEnabled(bool value)
{
    d->setPreviewsEnabled(value);
}

void RightPanelModelsAdapter::requestFetch(bool immediately)
{
    d->requestFetch(immediately);
}

bool RightPanelModelsAdapter::fetchInProgress() const
{
    return d->fetchInProgress();
}

QVector<RightPanel::EventCategory> RightPanelModelsAdapter::eventCategories() const
{
    using RightPanel::EventCategory;

    static const QVector<EventCategory> categories({
        {tr("Analytic"), "analytics.svg", EventCategory::Analytics},
        {tr("Generic"), "generic.svg", EventCategory::Generic},
        {tr("Input Signal"), "input.svg", EventCategory::Input},
        {tr("Soft Trigger"), "trigger.svg", EventCategory::SoftTrigger},
        {tr("Stream Issue"), "stream.svg", EventCategory::StreamIssue},
        {tr("Device Disconnect"), "turnoff.svg", EventCategory::DeviceDisconnect},
        {tr("Device IP Conflict"), "ip.svg", EventCategory::DeviceIpConflict}});

    return categories;
}

QAbstractItemModel* RightPanelModelsAdapter::analyticsEvents() const
{
    return d->analyticsEvents();
}

QStringList RightPanelModelsAdapter::flattenAttributeList(const analytics::AttributeList& source)
{
    QStringList result;
    result.reserve(source.size() * 2);

    for (const auto& attribute: source)
        result << attribute.displayedName << Private::valuesText(attribute.displayedValues);

    return result;
}

void RightPanelModelsAdapter::registerQmlTypes()
{
    qmlRegisterType<RightPanelModelsAdapter>("nx.vms.client.desktop", 1, 0, "RightPanelModel");

    qmlRegisterUncreatableType<AnalyticsSearchSetup>("nx.vms.client.desktop", 1, 0,
        "AnalyticsSearchSetup", "Cannot create instance of AnalyticsSearchSetup");

    qmlRegisterUncreatableType<CommonObjectSearchSetup>("nx.vms.client.desktop", 1, 0,
        "CommonObjectSearchSetup", "Cannot create instance of CommonObjectSearchSetup");

    qmlRegisterUncreatableType<TextFilterSetup>("nx.vms.client.desktop", 1, 0, "TextFilterSetup",
        "Cannot create instance of TextFilterSetup");

    qRegisterMetaType<QVector<RightPanel::EventCategory>>();
}

// ------------------------------------------------------------------------------------------------
// RightPanelModelsAdapter::Private

RightPanelModelsAdapter::Private::Private(RightPanelModelsAdapter* q):
    q(q)
{
    connect(q, &QAbstractListModel::modelAboutToBeReset, this,
        [this]
        {
            m_deadlines.clear();
            m_previews.clear();
            m_visibleItems.clear();
        });

    connect(q, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int row = first; row <= last; ++row)
                m_deadlines.remove(this->q->index(row, 0));

            m_previews.erase(m_previews.begin() + first, m_previews.begin() + (last + 1));

            QSet<QPersistentModelIndex> toRemove;
            for (const auto& index: m_visibleItems)
            {
                if (index.row() >= first && index.row() <= last)
                    toRemove.insert(index);
            }

            m_visibleItems -= toRemove;
        });

    connect(q, &QAbstractListModel::modelReset, this,
        [this]
        {
            const auto count = this->q->rowCount();
            createDeadlines(0, count);
            createPreviews(0, count);
        });

    connect(q, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            if (!NX_ASSERT(!parent.isValid()))
                return;

            const auto count = last - first + 1;
            createDeadlines(first, count);
            createPreviews(first, count);
        });

    m_autoCloseTimer.callOnTimeout(this, &Private::closeExpired);
    m_autoCloseTimer.start(1s);

    m_hangedPreviewRequestsChecker.callOnTimeout(this,
        [this]()
        {
            QSet<ResourceThumbnailProvider*> hangedProviders;
            for (const auto& [provider, data]: nx::utils::constKeyValueRange(m_loadingByProvider))
            {
                if (data.timeout.hasExpired())
                    hangedProviders.insert(provider);
            }

            for (auto provider: hangedProviders)
                handlePreviewLoadingEnded(provider);
        });

    m_hangedPreviewRequestsChecker.start(10s);

    m_fetchOperation.setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_fetchOperation.setInterval(kQueuedFetchDelay);
    m_fetchOperation.setCallback(
        [this]()
        {
            if (const auto model = searchModel(); model && model->canFetchData())
                model->fetchData();
        });

    const auto loadNextPreviewDelayed =
        [this]() { executeLater([this]() { loadNextPreview(); }, this); };

    m_previewLoadOperation.setFlags(nx::utils::PendingOperation::FireImmediately);
    m_previewLoadOperation.setInterval(kPreviewCheckInterval);
    m_previewLoadOperation.setCallback(loadNextPreviewDelayed);

    connect(q, &RightPanelModelsAdapter::fetchFinished,
        this, &Private::updateIsPlaceholderRequired);

    connect(q, &RightPanelModelsAdapter::isOnlineChanged,
        q, &RightPanelModelsAdapter::allowanceChanged);
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
}

void RightPanelModelsAdapter::Private::setType(Type value)
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
        case Type::motion:
        {
            // Prohibited for live viewers but should be allowed when browsing local files offline.
            return !q->isOnline() || accessController->isDeviceAccessRelevant(
                nx::vms::api::AccessRight::viewArchive);
        }

        case Type::bookmarks:
        {
            return q->isOnline() && accessController->isDeviceAccessRelevant(
                nx::vms::api::AccessRight::viewBookmarks);
        }

        case Type::events:
        {
            return q->isOnline()
                && accessController->hasGlobalPermissions(GlobalPermission::viewLogs);
        }

        case Type::analytics:
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

        case Type::notifications:
            return true;

        case Type::invalid:
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

    switch (m_type)
    {
        case Type::invalid:
            break;

        case Type::notifications:
            q->setSourceModel(new NotificationTabModel(m_context, this));
            break;

        case Type::motion:
            q->setSourceModel(new SimpleMotionSearchListModel(m_context, this));
            break;

        case Type::bookmarks:
            q->setSourceModel(new BookmarkSearchListModel(m_context, this));
            break;

        // TODO: #amalov Investigate the need of event model and default types here.
        case Type::events:
        {
            const auto eventsModel = new EventSearchListModel(m_context, this);
            q->setSourceModel(eventsModel);

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

        case Type::analytics:
        {
            const auto analyticsModel = new AnalyticsSearchListModel(m_context, this);
            analyticsModel->setLiveProcessingMode(
                AnalyticsSearchListModel::LiveProcessingMode::manualAdd);
            m_analyticsSetup.reset(new AnalyticsSearchSetup(analyticsModel));
            q->setSourceModel(analyticsModel);

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
            [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
            {
                for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
                    updatePreviewProvider(row);
            });
    }

    if (auto model = searchModel())
    {
        m_commonSetup->setModel(model);

        m_modelConnections << connect(model, &AbstractSearchListModel::dataNeeded, this,
            [this]()
            {
                updateIsConstrained();
                updateIsPlaceholderRequired();
                emit q->dataNeeded();
            });

        m_modelConnections << connect(model, &AbstractSearchListModel::liveAboutToBeCommitted,
            q, &RightPanelModelsAdapter::liveAboutToBeCommitted);

        m_modelConnections << connect(model, &AbstractSearchListModel::fetchCommitStarted,
            q, &RightPanelModelsAdapter::fetchCommitStarted);

        m_modelConnections << connect(model, &AbstractSearchListModel::fetchFinished,
            q, &RightPanelModelsAdapter::fetchFinished);

        m_modelConnections << connect(model, &AbstractSearchListModel::isOnlineChanged,
            q, &RightPanelModelsAdapter::isOnlineChanged);

        if (auto asyncModel = qobject_cast<AbstractAsyncSearchListModel*>(model))
        {
            m_modelConnections
                << connect(asyncModel, &AbstractAsyncSearchListModel::asyncFetchStarted,
                    q, &RightPanelModelsAdapter::asyncFetchStarted);
        }
    }

    updateSynchronizer();

    analyticsSetupGuard.fire();
    isOnlineGuard.fire();

    updateIsConstrained();
    updateItemCount();
}

void RightPanelModelsAdapter::Private::setPreviewsEnabled(bool value)
{
    if (value == m_previewsEnabled)
        return;

    m_previewsEnabled = value;
    emit q->previewsEnabledChanged();

    if (m_previewsEnabled)
        m_previewLoadOperation.requestOperation();
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

    emit q->dataChanged(q->index(0, 0), q->index(count - 1, 0), {Private::HighlightedRole});
}

bool RightPanelModelsAdapter::Private::isHighlighted(const QModelIndex& index) const
{
    if (m_highlightedTimestamp <= 0ms || m_highlightedResources.empty())
        return false;

    const auto timestamp = index.data(Qn::TimestampRole).value<microseconds>();
    if (timestamp <= 0us)
        return false;

    const auto duration = index.data(Qn::DurationRole).value<microseconds>();
    if (duration <= 0us)
        return false;

    const auto isHighlightedResource =
        [this](const QnResourcePtr& res) { return m_highlightedResources.contains(res); };

    const auto resources = index.data(Qn::ResourceListRole).value<QnResourceList>();
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
        case Type::motion:
        {
            const auto model = qobject_cast<SimpleMotionSearchListModel*>(q->sourceModel());
            m_synchronizer.reset(new MotionSearchSynchronizer(m_context, m_commonSetup, model));
            break;
        }

        case Type::bookmarks:
        {
            m_synchronizer.reset(new BookmarkSearchSynchronizer(m_context, m_commonSetup));
            break;
        }

        case Type::analytics:
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

void RightPanelModelsAdapter::Private::updateItemCount()
{
    const int value = q->sourceModel() ? q->sourceModel()->rowCount() : 0;
    if (value == m_itemCount)
        return;

    m_itemCount = value;
    emit q->itemCountChanged();

    updateIsPlaceholderRequired();
}

void RightPanelModelsAdapter::Private::updateIsPlaceholderRequired()
{
    const bool placeholderRequired = m_itemCount == 0 && !canFetch();
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

    for (const auto index: expired)
        q->removeRow(index.row());

    NX_VERBOSE(q, "Expired %1 tiles", expired.size());
    NX_ASSERT(expired.size() == (oldDeadlineCount - m_deadlines.size()));
}

void RightPanelModelsAdapter::Private::createPreviews(int first, int count)
{
    if (!NX_ASSERT(first >= 0 && first <= (int) m_previews.size()))
        return;

    std::vector<PreviewData> inserted(count);
    m_previews.insert(m_previews.begin() + first,
        std::make_move_iterator(inserted.begin()), std::make_move_iterator(inserted.end()));

    for (int i = 0; i < count; ++i)
        updatePreviewProvider(first + i);
}

void RightPanelModelsAdapter::Private::updatePreviewProvider(int row)
{
    if (!NX_ASSERT(row >= 0
        && row < (int) m_previews.size()
        && (int) m_previews.size() == q->rowCount()))
    {
        return;
    }

    const auto index = q->index(row, 0);
    const auto previewResource = index.data(Qn::ResourceRole).value<QnResource*>();
    const auto mediaResource = dynamic_cast<QnMediaResource*>(previewResource);

    if (!mediaResource || !previewResource->hasFlags(Qn::video))
    {
        m_previews[row] = {};
        return;
    }

    const auto previewTimeMs =
        duration_cast<milliseconds>(index.data(Qn::PreviewTimeRole).value<microseconds>());

    const auto rotation = mediaResource->forcedRotation().value_or(0);
    const auto highlightRect = nx::vms::client::core::Geometry::rotatedRelativeRectangle(
        index.data(Qn::ItemZoomRectRole).value<QRectF>(), -rotation / 90);

    const bool precisePreview = !highlightRect.isEmpty()
        || index.data(Qn::ForcePrecisePreviewRole).toBool();

    const auto objectTrackId = index.data(Qn::ObjectTrackIdRole).value<QnUuid>();

    nx::api::ResourceImageRequest request;
    request.resource = previewResource->toSharedPointer();
    request.timestampMs =
        previewTimeMs.count() > 0 ? previewTimeMs : nx::api::ImageRequest::kLatestThumbnail;
    request.rotation = rotation;
    request.size = QSize(kDefaultThumbnailWidth, 0);
    request.format = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;
    request.streamSelectionMode = index.data(Qn::PreviewStreamSelectionRole)
        .value<nx::api::ImageRequest::StreamSelectionMode>();
    request.objectTrackId = objectTrackId;

    calculatePreviewRects(request.resource, highlightRect, request.crop,
        m_previews[row].highlightRect);

    auto& previewProvider = m_previews[row].provider;
    if (!previewProvider || request.resource != previewProvider->requestData().resource)
    {
        previewProvider.reset(new ResourceThumbnailProvider(request));
        const auto id = providerId(previewProvider.get());
        previewsById()[id] = previewProvider.get();

        connect(previewProvider.get(), &QObject::destroyed, this,
            [this, provider = previewProvider.get()]() { handlePreviewLoadingEnded(provider); },
            Qt::QueuedConnection);

        connect(previewProvider.get(), &ImageProvider::statusChanged, this,
            [this, provider = previewProvider.get()](Qn::ThumbnailStatus status)
            {
                if (provider == m_providerLoadingFromCache)
                    return;

                switch (status)
                {
                    case Qn::ThumbnailStatus::Loading:
                    case Qn::ThumbnailStatus::Refreshing:
                    {
                        const auto server = previewServer(provider);
                        m_loadingByProvider.insert(provider, {server, kPreviewLoadTimeout});

                        auto& serverData = m_loadingByServer[server];
                        ++serverData.loadingCounter;

                        NX_ASSERT(serverData.loadingCounter <= maxSimultaneousPreviewLoads(server));
                        break;
                    }

                    default:
                    {
                        handlePreviewLoadingEnded(provider);
                        break;
                    }
                }
            });
    }
    else
    {
        const auto oldRequest = previewProvider->requestData();
        const bool forceUpdate = oldRequest.timestampMs != request.timestampMs
            || oldRequest.streamSelectionMode != request.streamSelectionMode;

        previewProvider->setRequestData(request, forceUpdate);
    }

    previewProvider->setProperty(kBypassVideoCachePropertyName,
        index.data(Qn::HasExternalBestShotRole).toBool());

    if (m_previewsEnabled && waitingStatus(m_previews[row]) != WaitingStatus::idle)
        m_previewLoadOperation.requestOperation();
}

QString RightPanelModelsAdapter::Private::previewId(int row) const
{
    const auto& provider = m_previews[row].provider;
    return provider && !provider->image().isNull()
        ? QString::number(providerId(provider.get()))
        : QString();
}

qreal RightPanelModelsAdapter::Private::previewAspectRatio(int row) const
{
    const auto& provider = m_previews[row].provider;
    const QSizeF size = provider ? provider->sizeHint() : QSizeF();
    return (size.height()) > 0 ? (size.width() / size.height()) : 1.0;
}

QRectF RightPanelModelsAdapter::Private::previewHighlightRect(int row) const
{
    const auto& provider = m_previews[row].provider;
    if (provider && QnLexical::deserialized<bool>(
        provider->image().text(CameraThumbnailProvider::kFrameFromPluginKey)))
    {
        return QRectF();
    }

    return m_previews[row].highlightRect;
}

RightPanel::PreviewState RightPanelModelsAdapter::Private::previewState(int row) const
{
    if (const auto& provider = m_previews[row].provider)
    {
        if (!provider->image().isNull())
            return RightPanel::PreviewState::ready;

        switch (provider->status())
        {
            case Qn::ThumbnailStatus::Invalid:
                return RightPanel::PreviewState::initial;

            case Qn::ThumbnailStatus::Loading:
            case Qn::ThumbnailStatus::Refreshing:
                return RightPanel::PreviewState::busy;

            case Qn::ThumbnailStatus::Loaded:
                return RightPanel::PreviewState::ready;

            case Qn::ThumbnailStatus::NoData:
                return RightPanel::PreviewState::missing;
        }
    }

    return RightPanel::PreviewState::missing;
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

bool RightPanelModelsAdapter::Private::isVisible(const QModelIndex& index) const
{
    return m_visibleItems.contains(index);
}

bool RightPanelModelsAdapter::Private::setVisible(const QModelIndex& index, bool value)
{
    if (!NX_ASSERT(index.isValid()) || isVisible(index) == value)
        return false;

    if (value)
    {
        m_visibleItems.insert(index);

        if (m_previewsEnabled
            && waitingStatus(m_previews[index.row()]) == WaitingStatus::requiresLoading)
        {
            m_previewLoadOperation.requestOperation();
        }

        const auto iter = m_deadlines.find(q->index(index.row(), 0));
        if (iter != m_deadlines.cend())
            iter->timer.setRemainingTime(kVisibleAutoCloseDelay);
    }
    else
    {
        m_visibleItems.remove(index);
    }

    return true;
}

void RightPanelModelsAdapter::Private::loadNextPreview()
{
    if (!m_previewsEnabled)
        return;

    NX_VERBOSE(q, "Trying to load next preview if needed");

    std::set<int> rowsToLoad;
    bool awaitingReload = false;

    for (const auto& index: m_visibleItems)
    {
        if (!NX_ASSERT(index.isValid()))
            continue;

        const auto status = waitingStatus(m_previews[index.row()]);
        if (status == WaitingStatus::requiresLoading)
            rowsToLoad.insert(index.row());
        else if (status == WaitingStatus::awaitsReloading)
            awaitingReload = true;
    }

    int loadedFromCache = 0;
    for (int row: rowsToLoad)
    {
        const auto provider = m_previews[row].provider.get();

        if (!provider->property(kBypassVideoCachePropertyName).toBool())
        {
            QScopedValueRollback<ResourceThumbnailProvider*> loadingFromCacheGuard(
                m_providerLoadingFromCache, provider);

            if (provider->tryLoad())
            {
                NX_VERBOSE(q, "Loaded preview from videocache (timestamp=%1, objectTrackId=%2",
                    provider->requestData().timestampMs,
                    provider->requestData().objectTrackId);

                ++loadedFromCache;
                continue;
            }
        }

        if (!isNextPreviewLoadAllowed(provider))
            continue;

        NX_VERBOSE(q, "Requesting preview from server (timestamp=%1, objectTrackId=%2",
            provider->requestData().timestampMs,
            provider->requestData().objectTrackId);

        requestPreview(row);
        m_sinceLastPreviewRequest.restart();
    }

    if (awaitingReload
        || (((int) rowsToLoad.size() > loadedFromCache) && m_loadingByProvider.empty()))
    {
        m_previewLoadOperation.requestOperation();
    }
}

bool RightPanelModelsAdapter::Private::isNextPreviewLoadAllowed(
    const ResourceThumbnailProvider* provider) const
{
    if (!NX_ASSERT(provider))
        return false;

    const auto server = previewServer(provider);
    return (m_loadingByServer.value(server).loadingCounter < maxSimultaneousPreviewLoads(server))
        && (tilePreviewLoadInterval() <= 0ms || m_sinceLastPreviewRequest.hasExpired(
            tilePreviewLoadInterval()));
}

void RightPanelModelsAdapter::Private::requestPreview(int row)
{
    const QPersistentModelIndex index(q->index(row, 0));
    auto& preview = m_previews[row];

    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(preview.provider.get(), &ImageProvider::statusChanged, this,
        [this, connection, index](Qn::ThumbnailStatus status)
        {
            if (isBusy(status))
                return;

            QObject::disconnect(*connection);
            if (!index.isValid())
                return;

            static const QVector<int> roles({
                Private::PreviewIdRole,
                Private::PreviewStateRole,
                Private::PreviewAspectRatioRole,
                Private::PreviewHighlightRectRole});

            emit q->dataChanged(index, index, roles);
        });

    NX_VERBOSE(q, "Load preview for row=%1", row);

    preview.provider->loadAsync();
    preview.reloadTimer.setRemainingTime(previewReloadDelay());
}

void RightPanelModelsAdapter::Private::handlePreviewLoadingEnded(
    ResourceThumbnailProvider* provider)
{
    const auto previewData = m_loadingByProvider.find(provider);
    if (previewData == m_loadingByProvider.end())
        return;

    const auto serverData = m_loadingByServer.find(previewData->server);
    if (NX_ASSERT(serverData != m_loadingByServer.end()))
    {
        --serverData->loadingCounter;
        NX_ASSERT(serverData->loadingCounter >= 0);
        if (serverData->loadingCounter <= 0)
            m_loadingByServer.erase(serverData);
    }

    m_loadingByProvider.erase(previewData);
    m_previewLoadOperation.requestOperation();
}

RightPanelModelsAdapter::Private::WaitingStatus RightPanelModelsAdapter::Private::waitingStatus(
    const PreviewData& data) const
{
    if (!data.provider)
        return WaitingStatus::idle;

    switch (data.provider->status())
    {
        case Qn::ThumbnailStatus::Invalid:
            return WaitingStatus::requiresLoading;

        case Qn::ThumbnailStatus::NoData:
            return data.reloadTimer.hasExpired()
                ? WaitingStatus::requiresLoading
                : WaitingStatus::awaitsReloading;

        default:
            return WaitingStatus::idle;
    }
}

void RightPanelModelsAdapter::Private::requestFetch(bool immediately)
{
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

bool RightPanelModelsAdapter::Private::canFetch() const
{
    const auto model = searchModel();
    return model && model->canFetchData();
}

AbstractSearchListModel* RightPanelModelsAdapter::Private::searchModel()
{
    return qobject_cast<AbstractSearchListModel*>(q->sourceModel());
}

const AbstractSearchListModel* RightPanelModelsAdapter::Private::searchModel() const
{
    return qobject_cast<const AbstractSearchListModel*>(q->sourceModel());
}

QString RightPanelModelsAdapter::Private::valuesText(const QStringList& values)
{
    // TODO: #vkutin Move this logic to the view side.

    const int totalCount = values.size();
    const QString displayedValues = nx::utils::strJoin(values.cbegin(),
        values.cbegin() + std::min(totalCount, kMaxDisplayedGroupValues),
        ", ");

    const int remainder = totalCount - kMaxDisplayedGroupValues;
    if (remainder <= 0)
        return displayedValues;

    return nx::format("%1 <font color=\"%3\">(%2)</font>", displayedValues,
        tr("+%n values", "", remainder),
        core::colorTheme()->color("light16").name());
}

void RightPanelModelsAdapter::Private::calculatePreviewRects(const QnResourcePtr& resource,
    const QRectF& highlightRect, QRectF& cropRect, QRectF& newHighlightRect)
{
    if (!highlightRect.isValid() || !NX_ASSERT(resource))
    {
        cropRect = {};
        newHighlightRect = {};
        return;
    }

    static QnAspectRatio kPreviewAr(16, 9);

    const auto resourceAr = nx::vms::client::core::AbstractResourceThumbnail::calculateAspectRatio(
        resource, kPreviewAr);

    nx::vms::client::desktop::calculatePreviewRects(
        resourceAr, highlightRect, kPreviewAr, cropRect, newHighlightRect);
}

void RightPanelModelsAdapter::Private::ensureAnalyticsRowVisible(int row)
{
    const auto synchronizer = qobject_cast<AnalyticsSearchSynchronizer*>(m_synchronizer.get());
    const auto index = q->index(row, 0);
    if (!NX_ASSERT(synchronizer && m_analyticsSetup && index.isValid()))
        return;

    const auto timestamp = index.data(Qn::TimestampRole).value<microseconds>();
    const auto trackId = index.data(Qn::ObjectTrackIdRole).value<QnUuid>();
    synchronizer->ensureVisible(duration_cast<milliseconds>(timestamp), trackId,
        m_analyticsSetup->model()->fetchedTimeWindow());
}

// ------------------------------------------------------------------------------------------------

QImage RightPanelImageProvider::requestImage(
    const QString& id, QSize* /*size*/, const QSize& /*requestedSize*/)
{
    const auto provider = previewsById().value(id.toLongLong());
    return provider ? provider->image() : QImage();
}

} // namespace nx::vms::client::desktop

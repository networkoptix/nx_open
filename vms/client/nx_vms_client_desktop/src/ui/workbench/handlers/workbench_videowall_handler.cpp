// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_videowall_handler.h"

#include <algorithm>

#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QPushButton>

#include <api/runtime_info_manager.h>
#include <client/client_installations_manager.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/build_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/json.h>
#include <nx/utils/counter.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_resource_helpers.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_process_runner.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/ui/messages/videowall_messages.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/videowall/desktop_camera_connection_controller.h>
#include <nx/vms/client/desktop/videowall/desktop_camera_stub_controller.h>
#include <nx/vms/client/desktop/videowall/utils.h>
#include <nx/vms/client/desktop/videowall/workbench_videowall_shortcut_helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/serialization/qt_gui_types.h>
#include <nx/vms/license/usage_helper.h>
#include <nx/vms/utils/platform/autorun.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <platform/platform_abstraction.h>
#include <recording/time_period.h>
#include <ui/dialogs/attach_to_videowall_dialog.h>
#include <ui/dialogs/layout_name_dialog.h> //< TODO: #sivanov Invalid reuse.
#include <ui/dialogs/resource_properties/videowall_settings_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/widgets/word_wrapped_label.h>
#include <ui/workbench/extensions/workbench_layout_change_validator.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/applauncher_utils.h>
#include <utils/color_space/image_correction.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/uuid_pool.h>
#include <utils/screen_snaps_geometry.h>
#include <utils/screen_utils.h>
#include <utils/unity_launcher_workaround.h>

using nx::vms::client::core::LayoutResource;
using nx::vms::client::core::LayoutResourcePtr;
using nx::vms::client::core::LayoutResourceList;
using nx::vms::client::core::MotionSelection;
using nx::vms::common::LayoutItemData;

using namespace std::chrono;
using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

#define PARAM_KEY(KEY) const QLatin1String KEY##Key(BOOST_PP_STRINGIZE(KEY));
PARAM_KEY(sequence)
PARAM_KEY(pcUuid)
PARAM_KEY(uuid)
PARAM_KEY(zoomUuid)
PARAM_KEY(resource)
PARAM_KEY(value)
PARAM_KEY(role)
PARAM_KEY(rotation)
PARAM_KEY(speed)
PARAM_KEY(geometry)
PARAM_KEY(zoomRect)
PARAM_KEY(checkedButtons)
PARAM_KEY(items)
#undef PARAM_KEY

static const QString kPositionUsecKey("position");
static const QString kSilentKey("silent");

QnVideoWallItemIndexList getIndices(
    const LayoutResourcePtr& layout,
    const LayoutItemData& data)
{
    return layout->itemData(data.uuid, Qn::VideoWallItemIndicesRole)
        .value<QnVideoWallItemIndexList>();
}

void setIndices(
    const LayoutResourcePtr& layout,
    const LayoutItemData& data,
    const QnVideoWallItemIndexList& value)
{
    layout->setItemData(data.uuid, Qn::VideoWallItemIndicesRole, QVariant::fromValue(value));
}

void addItemToLayout(const LayoutResourcePtr& layout, const QnVideoWallItemIndexList& indices)
{
    if (indices.isEmpty())
        return;

    QnVideoWallItemIndex firstIdx = indices.first();
    if (!firstIdx.isValid())
        return;

    QList<int> screens =
        screensCoveredByItem(firstIdx.item(), firstIdx.videowall()).values();
    std::sort(screens.begin(), screens.end());
    if (screens.isEmpty())
        return;

    QnVideoWallPcData pc = firstIdx.videowall()->pcs()->getItem(firstIdx.item().pcUuid);
    if (screens.first() >= pc.screens.size())
        return;

    QRect geometry = pc.screens[screens.first()].layoutGeometry;
    if (geometry.isValid())
    {
        for (const LayoutItemData &item : layout->getItems())
        {
            if (!item.combinedGeometry.isValid())
                continue;
            if (!item.combinedGeometry.intersects(geometry))
                continue;
            geometry = QRect(); //invalidate selected geometry
            break;
        }
    }

    LayoutItemData itemData;
    itemData.uuid = nx::Uuid::createUuid();
    itemData.combinedGeometry = geometry;
    if (geometry.isValid())
        itemData.flags = Qn::Pinned;
    else
        itemData.flags = Qn::PendingGeometryAdjustment;
    itemData.resource.id = firstIdx.videowall()->getId();
    setIndices(layout, itemData, indices);
    layout->addItem(itemData);
}

struct ScreenWidgetKey
{
    nx::Uuid pcUuid;
    QSet<int> screens;

    ScreenWidgetKey(const nx::Uuid &pcUuid, const QSet<int> screens):
        pcUuid(pcUuid), screens(screens)
    {
    }

    friend bool operator<(const ScreenWidgetKey &l, const ScreenWidgetKey &r)
    {
        if (l.pcUuid != r.pcUuid || (l.screens.isEmpty() && r.screens.isEmpty()))
            return l.pcUuid < r.pcUuid;
        auto lmin = std::min_element(l.screens.constBegin(), l.screens.constEnd());
        auto rmin = std::min_element(r.screens.constBegin(), r.screens.constEnd());
        return (*lmin) < (*rmin);
    }
};

static constexpr auto kIdentifyTimeout = 5s;
static constexpr int kIdentifyFontSize = 100;

const int cacheMessagesTimeoutMs = 500;

const float defaultReviewAR = 1920.0f / 1080.0f;

const nx::Uuid uuidPoolBase("621992b6-5b8a-4197-af04-1657baab71f0");

class QnVideowallReviewLayoutResource:
    public LayoutResource,
    public QnWorkbenchContextAware
{
public:
    QnVideowallReviewLayoutResource(
        const QnVideoWallResourcePtr& videowall,
        QnWorkbenchContext* context)
        :
        LayoutResource(),
        QnWorkbenchContextAware(context),
        m_videowall(videowall)
    {
        addFlags(Qn::local);
        setName(videowall->getName());
        setPredefinedCellSpacing(core::CellSpacing::Medium);
        setCellAspectRatio(defaultReviewAR);
        setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission));
        setData(Qn::VideoWallResourceRole, QVariant::fromValue(videowall));

        connect(videowall.data(), &QnResource::nameChanged, this,
            [this](const QnResourcePtr &resource) { setName(resource->getName()); });
    }

    virtual bool doSaveAsync(SaveLayoutResultFunction callback) override
    {
        const auto connection = messageBusConnection();
        if (!NX_ASSERT(connection))
            return false;

        const auto thisPtr = toSharedPointer(this);
        const auto workbenchLayout = workbench()->layout(thisPtr);

        for (const auto workbenchItem: workbenchLayout->items())
        {
            const LayoutItemData data = workbenchItem->data();
            const QnVideoWallItemIndexList indices = getIndices(thisPtr, data);
            if (indices.isEmpty())
                continue;
            const QnVideoWallItemIndex firstIdx = indices.first();
            NX_ASSERT(firstIdx.videowall() == m_videowall);
            const QnVideoWallItem item = firstIdx.item();
            const QSet<int> screenIndices = screensCoveredByItem(item, m_videowall);
            if (!m_videowall->pcs()->hasItem(item.pcUuid))
                continue;
            QnVideoWallPcData pc = m_videowall->pcs()->getItem(item.pcUuid);
            if (screenIndices.size() < 1)
                continue;
            for (int screenIndex: screenIndices)
                pc.screens[screenIndex].layoutGeometry = data.combinedGeometry.toRect();
            m_videowall->pcs()->updateItem(pc);
        }

        return qnResourcesChangesManager->saveVideoWall(m_videowall,
            [callback, layout = thisPtr](bool success, const QnVideoWallResourcePtr& videowall)
            {
                NX_ASSERT(videowall == layout->m_videowall);

                if (callback)
                    callback(success, layout);
            });
    }

private:
    QnVideoWallResourcePtr m_videowall;
};

auto offlineItemOnThisPc = []
    {
        nx::Uuid pcUuid = appContext()->localSettings()->pcUuid();
        NX_ASSERT(!pcUuid.isNull(), "Invalid pc state.");
        return [pcUuid](const QnVideoWallItem& item)
            {
                return !pcUuid.isNull() && item.pcUuid == pcUuid && !item.runtimeStatus.online;
            };
    };

QSet<nx::Uuid> layoutResourceIds(const QnLayoutResourcePtr& layout)
{
    QSet<nx::Uuid> result;
    for (const auto& item: layout->getItems())
        result << item.resource.id;
    return result;
}

// Main window geometry should have position in physical coordinates and size - in logical.
QRect mainWindowGeometry(const QnScreenSnaps& snaps, QWidget* mainWindowWidget)
{
    if (!snaps.isValid())
        return {};

    return screenSnapsToWidgetGeometry(snaps, mainWindowWidget);
}

nx::Url serverUrl(const nx::vms::client::core::RemoteConnectionPtr& connection)
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(connection->address())
        .toUrl();
}

nx::Url serverUrl(const QnResourcePtr& resource)
{
    if (const auto context = resource->systemContext())
    {
        if (const auto connection = context->as<nx::vms::client::core::SystemContext>()->connection())
            return serverUrl(connection);
    }

    return {};
}

} // namespace

//--------------------------------------------------------------------------------------------------

QnWorkbenchVideoWallHandler::QnWorkbenchVideoWallHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_licensesHelper(new nx::vms::license::VideoWallLicenseUsageHelper(systemContext(), this)),
    #ifdef _DEBUG
        /* Limit by reasonable size. */
        m_uuidPool(new UuidPool(uuidPoolBase, 256))
    #else
        m_uuidPool(new UuidPool(uuidPoolBase, 16384))
    #endif
{
    connect(windowContext(), &WindowContext::beforeSystemChanged,
        [this]
        {
            if (!m_localDesktopCamera || !ini().enableDesktopCameraLazyInitialization)
                return;

            QSet<QnVideoWallResourcePtr> videowallsToSave;
            for (const nx::Uuid& videoWallId: m_videoWallsWithLocalDesktopCamera)
            {
                const auto videoWall =
                    resourcePool()->getResourceById<QnVideoWallResource>(videoWallId);

                if (!videoWall)
                    continue;

                for (QnVideoWallItem videoWallItem: videoWall->items()->getItems())
                {
                    if (!m_localDesktopCameraLayoutIds.contains(videoWallItem.layout))
                        continue;

                    const auto layout =
                        resourcePool()->getResourceById<QnLayoutResource>(videoWallItem.layout);

                    const nx::vms::common::LayoutItemDataMap layoutItems = layout->getItems();
                    if (layoutItems.size() != 1)
                        continue; //< Desktop camera is always the only one item of a layout.

                    if (layoutItems.begin()->resource.id != m_localDesktopCamera->getId())
                        continue; //< Layout was changed/removed since "Push my screen" activation.

                    videoWallItem.layout = nx::Uuid();

                    QnVideoWallItemIndex itemIndex = resourcePool()->getVideoWallItemByUuid(
                        videoWallItem.uuid);

                    itemIndex.videowall()->items()->updateItem(videoWallItem);
                    videowallsToSave << videoWall;
                }
            }

            saveVideowalls(videowallsToSave, /*saveLayout*/ false,
                [this](bool successfullySaved, const QnVideoWallResourcePtr& videowall)
                {
                    if (successfullySaved)
                        cleanupUnusedLayouts({videowall});
                });

            m_videoWallsWithLocalDesktopCamera.clear();
            m_localDesktopCameraLayoutIds.clear();

            qnResourcesChangesManager->deleteResource(m_localDesktopCamera);
            m_localDesktopCamera.clear();
        });

    m_licensesHelper->setCustomValidator(
        std::make_unique<license::VideoWallLicenseValidator>(systemContext()));

    m_videoWallMode.active = qnRuntime->isVideoWallMode();
    m_videoWallMode.opening = false;
    m_videoWallMode.ready = false;
    m_controlMode.active = false;
    m_controlMode.sequence = 0;

    m_controlMode.cacheTimer = new QTimer(this);
    m_controlMode.cacheTimer->setInterval(cacheMessagesTimeoutMs);
    connect(m_controlMode.cacheTimer, &QTimer::timeout, this,
        &QnWorkbenchVideoWallHandler::at_controlModeCacheTimer_timeout);
    m_controlMode.syncPlayTimer = new QTimer(this);
    m_controlMode.syncPlayTimer->start(30000); //< We believe one sync in 30s is reasonable.
    connect(m_controlMode.syncPlayTimer, &QTimer::timeout, this,
        [this]{ syncTimelinePosition(/*silent*/ true); });

    nx::Uuid pcUuid = appContext()->localSettings()->pcUuid();
    if (pcUuid.isNull())
    {
        pcUuid = nx::Uuid::createUuid();
        appContext()->localSettings()->pcUuid = pcUuid;
    }
    m_controlMode.pcUuid = pcUuid.toString(QUuid::WithBraces);

    /* Common connections */
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchVideoWallHandler::at_resPool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchVideoWallHandler::at_resPool_resourceRemoved);
    for (const QnResourcePtr& resource: resourcePool()->getResources())
        at_resPool_resourceAdded(resource);

    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            if (info.data.peer.peerType == nx::vms::api::PeerType::videowallClient)
            {
                /* Master node, will run other clients and exit. */
                if (info.data.videoWallInstanceGuid.isNull())
                    return;

                setItemOnline(info.data.videoWallInstanceGuid, true);
            }
            else if (!info.data.videoWallControlSession.isNull())
            {
                setItemControlledBy(info.data.videoWallControlSession, info.uuid, true);
            }
        });

    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            if (info.data.peer.peerType == nx::vms::api::PeerType::videowallClient)
            {
                /* Master node, will run other clients and exit. */
                if (info.data.videoWallInstanceGuid.isNull())
                    return;

                setItemOnline(info.data.videoWallInstanceGuid, false);
            }
            else if (!info.data.videoWallControlSession.isNull())
            {
                setItemControlledBy(info.data.videoWallControlSession, info.uuid, false);
            }
        });

    /* Handle simultaneous control mode enter. */
    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo &info)
        {
            /* Ignore own info change. */
            if (info.uuid == peerId())
                return;

            bool isControlled = !info.data.videoWallControlSession.isNull();
            setItemControlledBy(info.data.videoWallControlSession, info.uuid, isControlled);

            /* Skip if we are not controlling videowall now. */
            if (!m_controlMode.active)
                return;

            auto layout = workbench()->currentLayout()->resource();

            /* Check the conflict. */
            if (info.data.videoWallControlSession.isNull() ||
                (layout && info.data.videoWallControlSession != layout->getId()))
                return;

            /* Order by guid. */
            if (info.uuid < peerId())
            {
                setControlMode(false);
                showControlledByAnotherUserMessage();
            }

        });

    for (const auto& info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.peer.peerType != nx::vms::api::PeerType::videowallClient)
            continue;

        /* Master node, will run other clients and exit. */
        if (info.data.videoWallInstanceGuid.isNull())
            continue;

        setItemOnline(info.data.videoWallInstanceGuid, true);
    }

    const auto clientMessageProcessor = qnClientMessageProcessor;
    connect(clientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        [this]() { cleanupUnusedLayouts(); });

    if (m_videoWallMode.active)
    {
        /* Videowall reaction actions */

        connect(action(menu::DelayedOpenVideoWallItemAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_delayedOpenVideoWallItemAction_triggered);

        connect(clientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
            [this]
            {
                if (m_videoWallMode.ready)
                    return;
                m_videoWallMode.ready = true;
                submitDelayedItemOpen();
            });
        connect(clientMessageProcessor, &QnCommonMessageProcessor::videowallControlMessageReceived,
            this, &QnWorkbenchVideoWallHandler::at_eventManager_controlMessageReceived);

    }
    else
    {

        /* Control videowall actions */

        connect(action(menu::NewVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered);
        connect(action(menu::RemoveFromServerAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_deleteVideoWallAction_triggered);
        connect(action(menu::AttachToVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered);
        connect(action(menu::ClearVideoWallScreen), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered);
        connect(action(menu::DeleteVideoWallItemAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_deleteVideoWallItemAction_triggered);
        connect(action(menu::StartVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_startVideoWallAction_triggered);
        connect(action(menu::StopVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_stopVideoWallAction_triggered);
        connect(action(menu::RenameVideowallEntityAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_renameAction_triggered);
        connect(action(menu::IdentifyVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_identifyVideoWallAction_triggered);
        connect(action(menu::StartVideoWallControlAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered);
        connect(action(menu::OpenVideoWallReviewAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_openVideoWallReviewAction_triggered);
        connect(action(menu::SaveCurrentVideoWallReviewAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_saveCurrentVideoWallReviewAction_triggered);
        connect(action(menu::SaveVideoWallReviewAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered);
        connect(action(menu::DropOnVideoWallItemAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_dropOnVideoWallItemAction_triggered);
        connect(action(menu::PushMyScreenToVideowallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_pushMyScreenToVideowallAction_triggered);
        connect(action(menu::VideowallSettingsAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_videowallSettingsAction_triggered);
        connect(action(menu::SaveVideowallMatrixAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_saveVideowallMatrixAction_triggered);
        connect(action(menu::LoadVideowallMatrixAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_loadVideowallMatrixAction_triggered);
        connect(action(menu::DeleteVideowallMatrixAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_deleteVideowallMatrixAction_triggered);
        connect(action(menu::VideoWallScreenSettingsAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_videoWallScreenSettingsAction_triggered);

        connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
            &QnWorkbenchVideoWallHandler::at_display_widgetAdded);
        connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
            &QnWorkbenchVideoWallHandler::at_display_widgetAboutToBeRemoved);

        connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
            &QnWorkbenchVideoWallHandler::at_workbench_currentLayoutAboutToBeChanged);
        connect(workbench(), &Workbench::currentLayoutChanged, this,
            &QnWorkbenchVideoWallHandler::at_workbench_currentLayoutChanged);

        connect(navigator(), &QnWorkbenchNavigator::positionChanged, this,
            [this] { syncTimelinePosition(/*silent*/ false); });
        connect(navigator(), &QnWorkbenchNavigator::playingChanged, this,
            &QnWorkbenchVideoWallHandler::at_navigator_playingChanged);
        connect(navigator(), &QnWorkbenchNavigator::speedChanged, this,
            &QnWorkbenchVideoWallHandler::at_navigator_speedChanged);

        connect(workbench()->windowContext()->streamSynchronizer(),
            &QnWorkbenchStreamSynchronizer::runningChanged,
            this,
            &QnWorkbenchVideoWallHandler::at_workbenchStreamSynchronizer_runningChanged);

        for (auto widget: display()->widgets())
            at_display_widgetAdded(widget);
    }
}

QnWorkbenchVideoWallHandler::~QnWorkbenchVideoWallHandler()
{
}

void QnWorkbenchVideoWallHandler::resetLayout(
    const QnVideoWallItemIndexList& items,
    const LayoutResourcePtr& layout)
{
    if (items.isEmpty())
        return;

    // Cloud layouts must be copied before placed on video wall.
    if (!NX_ASSERT(layout->systemContext() == system()))
        return;

    layout->setCellSpacing(0.0);

    const bool needToSave = !layout->isFile() && layout->canBeSaved();
    if (needToSave)
    {
        auto callback =
            [this, items, id = layout->getId()](bool success, const LayoutResourcePtr& /*layout*/)
            {
                if (!success)
                    showFailedToApplyChanges();
                else
                    updateItemsLayout(items, id);
                updateMode();
            };
        layout->saveAsync(callback);
        resourcePropertyDictionary()->saveParamsAsync(layout->getId());
    }
    else
    {
        updateItemsLayout(items, layout->getId());
    }
}

void QnWorkbenchVideoWallHandler::swapLayouts(
    const QnVideoWallItemIndex firstIndex,
    const LayoutResourcePtr& firstLayout,
    const QnVideoWallItemIndex& secondIndex,
    const LayoutResourcePtr& secondLayout)
{
    if (!firstIndex.isValid() || !secondIndex.isValid())
        return;

    LayoutResourceList unsavedLayouts;
    if (firstLayout && firstLayout->canBeSaved())
        unsavedLayouts << firstLayout;

    if (secondLayout && secondLayout->canBeSaved())
        unsavedLayouts << secondLayout;

    auto swap =
        [this](
            const QnVideoWallItemIndex firstIndex,
            const LayoutResourcePtr& firstLayout,
            const QnVideoWallItemIndex& secondIndex,
            const LayoutResourcePtr& secondLayout)
        {
            QnVideoWallItem firstItem = firstIndex.item();
            firstItem.layout = firstLayout ? firstLayout->getId() : nx::Uuid();
            firstIndex.videowall()->items()->updateItem(firstItem);

            QnVideoWallItem secondItem = secondIndex.item();
            secondItem.layout = secondLayout ? secondLayout->getId() : nx::Uuid();
            secondIndex.videowall()->items()->updateItem(secondItem);

            saveVideowalls(QSet<QnVideoWallResourcePtr>()
                << firstIndex.videowall()
                << secondIndex.videowall());
        };

    if (!unsavedLayouts.isEmpty())
    {
        auto callback =
            [this, firstIndex, firstLayout, secondIndex, secondLayout, swap]
            (bool success, const LayoutResourcePtr& /*layout*/ = {})
            {
                if (!success)
                    showFailedToApplyChanges();
                else
                    swap(firstIndex, firstLayout, secondIndex, secondLayout);
            };

        if (unsavedLayouts.size() == 1)
        {
            unsavedLayouts.first()->saveAsync(callback);
        }
        else
        {
            // Avoiding double swap.
            // TODO: #sivanov Simplify the logic.
            bool bothSuccess = true;
            auto counter = new nx::utils::CounterWithSignal(2);
            connect(counter, &nx::utils::CounterWithSignal::reachedZero, this,
                [callback, &bothSuccess, counter]()
                {
                    callback(bothSuccess);
                    counter->deleteLater();
                });

            auto localCallback =
                [&bothSuccess, counter](bool success, const LayoutResourcePtr& /*layout*/)
                {
                    bothSuccess &= success;
                    counter->decrement();
                };
            for (const LayoutResourcePtr& layout: unsavedLayouts)
                layout->saveAsync(localCallback);
        }
    }
    else
    {
        swap(firstIndex, firstLayout, secondIndex, secondLayout);
    }
}

void QnWorkbenchVideoWallHandler::updateItemsLayout(
    const QnVideoWallItemIndexList& items,
    const nx::Uuid& layoutId)
{
    QSet<QnVideoWallResourcePtr> videoWalls;

    for (const QnVideoWallItemIndex& index: items)
    {
        if (!index.isValid())
            continue;

        QnVideoWallItem existingItem = index.item();
        if (existingItem.layout == layoutId)
            continue;

        existingItem.layout = layoutId;
        index.videowall()->items()->updateItem(existingItem);
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls, /*saveLayout*/ false,
        [this](bool successfullySaved, const QnVideoWallResourcePtr& videowall)
        {
            if (successfullySaved)
                cleanupUnusedLayouts({videowall});
        });
}

bool QnWorkbenchVideoWallHandler::canStartVideowall(const QnVideoWallResourcePtr &videowall) const
{
    NX_ASSERT(videowall);
    if (!videowall)
        return false;

    const auto& values = videowall->items()->getItems().values();
    return std::any_of(values.begin(), values.end(), offlineItemOnThisPc());
}

void QnWorkbenchVideoWallHandler::switchToVideoWallMode(const QnVideoWallResourcePtr& videoWall)
{
    QList<QnVideoWallItem> items;
    const auto& values = videoWall->items()->getItems().values();
    std::copy_if(values.begin(), values.end(), std::back_inserter(items),
        offlineItemOnThisPc());

    NX_ASSERT(!items.isEmpty(), "Action condition must not allow us to get here.");
    if (items.isEmpty())
        return;

    bool closeCurrentInstance = false;
    if (!messages::Videowall::switchToVideoWallMode(mainWindowWidget(), &closeCurrentInstance))
        return;

    if (closeCurrentInstance)
        closeInstanceDelayed();

    for (const auto& item: items)
    {
        openNewWindow(videoWall->getId(), item);
    }
}

void QnWorkbenchVideoWallHandler::openNewWindow(
    nx::Uuid videoWallId, const QnVideoWallItem& item, const nx::utils::SoftwareVersion& version)
{
    using namespace nx::vms::applauncher::api;

    const auto connection = this->connection();

    if (!NX_ASSERT(connection, "Client must be connected while we are running the video wall"))
        return;

    // Connection credentials will be constructed by callee from videowall IDs.
    const auto url = serverUrl(connection);
    NX_VERBOSE(this, "Opening item id: %1, videowall: %2, server: %3, version: %4",
        item.uuid, videoWallId, url, version);

    QStringList arguments;
    arguments
        << "--videowall"
        << videoWallId.toString(QUuid::WithBraces)
        << "--videowall-instance"
        << item.uuid.toString(QUuid::WithBraces)
        << "--auth"
        << url.toString();

    if (version.isNull() || restartClient(version, /*auth*/ {}, arguments) != ResultType::ok)
        ClientProcessRunner().runClient(arguments);
}

void QnWorkbenchVideoWallHandler::openVideoWallItem(const QnVideoWallResourcePtr &videoWall)
{
    if (!videoWall)
    {
        NX_ERROR(this, "Warning: videowall not exists anymore, cannot open videowall item");
        closeInstanceDelayed();
        return;
    }

    QnVideoWallItem item = videoWall->items()->getItem(m_videoWallMode.instanceGuid);
    executeDelayedParented([this, item] { updateMainWindowGeometry(item.screenSnaps); }, this);

    QnLayoutResourcePtr layout = resourcePool()->getResourceById<QnLayoutResource>(item.layout);

    if (workbench()->currentLayout() && workbench()->currentLayout()->resource() == layout)
        return;

    workbench()->clear();
    // Action is disabled in videowall mode to make sure it is not displayed in the context menu.
    if (layout)
        menu()->triggerForced(menu::OpenInNewTabAction, layout);
}

void QnWorkbenchVideoWallHandler::closeInstanceDelayed()
{
    menu()->trigger(menu::DelayedForcedExitAction);
}

void QnWorkbenchVideoWallHandler::sendMessage(const QnVideoWallControlMessage& message, bool cached)
{
    NX_ASSERT(m_controlMode.active);

    auto connection = messageBusConnection();
    if (!NX_ASSERT(connection))
        return;

    auto videoWallManager = connection->getVideowallManager(nx::network::rest::kSystemSession);

    if (cached)
    {
        m_controlMode.cachedMessages << message;
        return;
    }

    QnVideoWallControlMessage localMessage(message);
    localMessage[sequenceKey] = QString::number(m_controlMode.sequence++);
    localMessage[pcUuidKey] = m_controlMode.pcUuid;

    nx::vms::api::VideowallControlMessageData apiMessage;
    ec2::fromResourceToApi(localMessage, apiMessage);

    for (const QnVideoWallItemIndex &index : targetList())
    {
        apiMessage.videowallGuid = index.videowall()->getId();
        apiMessage.instanceGuid = index.uuid();
        NX_VERBOSE(this, "SENDER: sending message %1: %2 to %3",
            m_controlMode.sequence,
            message,
            apiMessage.instanceGuid);
        videoWallManager->sendControlMessage(apiMessage, [](int /*requestId*/, ec2::ErrorCode) {});
    }
}

void QnWorkbenchVideoWallHandler::handleMessage(
    const QnVideoWallControlMessage& message,
    const nx::Uuid& controllerUuid,
    qint64 sequence)
{
    NX_VERBOSE(this, "RECEIVER: handling message %1", message);

    if (sequence >= 0)
        m_videoWallMode.sequenceByPcUuid[controllerUuid] = sequence;

    switch (static_cast<QnVideoWallControlMessage::Operation>(message.operation))
    {
        case QnVideoWallControlMessage::Exit:
        {
            closeInstanceDelayed();
            return;
        }
        case QnVideoWallControlMessage::ControlStarted:
        {
            //clear stored messages with lesser sequence
            StoredMessagesHash &stored = m_videoWallMode.storedMessages[controllerUuid];
            StoredMessagesHash::iterator i = stored.begin();
            while (i != stored.end())
            {
                if (i.key() < sequence)
                    i = stored.erase(i);
                else
                    ++i;
            }
            return;
        }
        case QnVideoWallControlMessage::ItemRoleChanged:
        {
            Qn::ItemRole role = static_cast<Qn::ItemRole>(message[roleKey].toInt());
            nx::Uuid guid(message[uuidKey]);
            if (guid.isNull())
                workbench()->setItem(role, nullptr);
            else
                workbench()->setItem(role, workbench()->currentLayout()->item(guid));
            break;
        }
        case QnVideoWallControlMessage::LayoutDataChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            Qn::ItemDataRole role = static_cast<Qn::ItemDataRole>(message[roleKey].toInt());
            switch (role)
            {
                case Qn::LayoutCellSpacingRole:
                {
                    auto data = QJson::deserialized<qreal>(value);
                    workbench()->currentLayoutResource()->setCellSpacing(data);
                    break;
                }
                case Qn::LayoutCellAspectRatioRole:
                {
                    qreal data;
                    QJson::deserialize(value, &data);
                    workbench()->currentLayoutResource()->setCellAspectRatio(data);
                    break;
                }
                default:
                    break;
            }

            break;
        }
        case QnVideoWallControlMessage::LayoutItemAdded:
        {
            QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
            if (!layout)
                return;

            nx::Uuid uuid = nx::Uuid(message[uuidKey]);
            if (workbench()->currentLayout()->item(uuid))
                return;

            nx::Uuid resourceId(message[resourceKey]);
            const auto resource = resourcePool()->getResourceById(resourceId);
            if (!resource)
                return;

            QRect geometry = QJson::deserialized<QRect>(message[geometryKey].toUtf8());
            QRectF zoomRect = QJson::deserialized<QRectF>(message[zoomRectKey].toUtf8());
            qreal rotation = QJson::deserialized<qreal>(message[rotationKey].toUtf8());
            int checkedButtons = QJson::deserialized<int>(message[checkedButtonsKey].toUtf8());
            auto item = new QnWorkbenchItem(resource, uuid, workbench()->currentLayout());
            item->setGeometry(geometry);
            item->setZoomRect(zoomRect);
            item->setRotation(rotation);
            item->setFlag(Qn::PendingGeometryAdjustment);
            // Item buttons will be restored when widget is created.
            workbench()->currentLayoutResource()->setItemData(
                uuid,
                Qn::ItemCheckedButtonsRole,
                checkedButtons);
            workbench()->currentLayout()->addItem(item);

            NX_VERBOSE(this, "RECEIVER: Item %1 added to %2", uuid, geometry);
            break;
        }
        case QnVideoWallControlMessage::LayoutItemRemoved:
        {
            nx::Uuid uuid = nx::Uuid(message[uuidKey]);
            if (QnWorkbenchItem* item = workbench()->currentLayout()->item(uuid))
                workbench()->currentLayout()->removeItem(item);
            break;
        }
        // Keep in sync with QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged.
        case QnVideoWallControlMessage::LayoutItemDataChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            nx::Uuid uuid = nx::Uuid(message[uuidKey]);
            auto item = workbench()->currentLayout()->item(uuid);
            if (!item)
                return;

            const int role = message[roleKey].toInt();

            switch (role)
            {
                case Qn::ItemGeometryRole:
                {
                    QRect data = QJson::deserialized<QRect>(value);
                    item->setData(role, data);
                    NX_VERBOSE(this, "RECEIVER: Item %1 geometry changed to %2", uuid, data);
                    break;
                }
                case core::ItemZoomRectRole:
                {
                    QRectF data = QJson::deserialized<QRectF>(value);
                    item->setData(role, data);
                    NX_VERBOSE(this, "RECEIVER: Item %1 %2 changed to %3",
                        uuid, QnLexical::serialized(role), data);
                    break;
                }
                case Qn::ItemZoomWindowRectangleVisibleRole:
                {
                    auto data = QJson::deserialized<bool>(value);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemPositionRole:
                {
                    QPointF data = QJson::deserialized<QPointF>(value);
                    item->setData(role, data);
                    menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
                    break;
                }
                case Qn::ItemRotationRole:
                {
                    qreal data;
                    QJson::deserialize(value, &data);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemSpeedRole:
                {
                    qreal data;
                    QJson::deserialize(value, &data);
                    item->setData(role, data);
                    menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
                    break;
                }
                case Qn::ItemFlipRole:
                {
                    bool data;
                    QJson::deserialize(value, &data);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemPausedRole:
                {
                    bool data;
                    QJson::deserialize(value, &data);
                    item->setData(role, data);
                    menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
                    break;
                }
                case Qn::ItemCheckedButtonsRole:
                {
                    int data;
                    QJson::deserialize(value, &data);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemFrameDistinctionColorRole:
                {
                    const auto data = nx::reflect::fromString<QColor>(value.toStdString());
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemSliderWindowRole:
                case Qn::ItemSliderSelectionRole:
                {
                    QnTimePeriod data = QJson::deserialized<QnTimePeriod>(value);
                    item->setData(role, data);
                    menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
                    break;
                }
                case Qn::ItemImageEnhancementRole:
                {
                    auto data = QJson::deserialized<nx::vms::api::ImageCorrectionData>(value);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemImageDewarpingRole:
                {
                    auto data = QJson::deserialized<nx::vms::api::dewarping::ViewData>(value);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemHealthMonitoringButtonsRole:
                {
                    auto data =
                        QJson::deserialized<QnServerResourceWidget::HealthMonitoringButtons>(value);
                    item->setData(role, data);
                    break;
                }

                default:
                    NX_ASSERT(this, "Unexpected item data change, role: %1",
                        QnLexical::serialized(role));
                    break;
            }

            break;
        }
        case QnVideoWallControlMessage::ZoomLinkAdded:
        {
            auto item = workbench()->currentLayout()->item(nx::Uuid(message[uuidKey]));
            auto zoomTargetItem = workbench()->currentLayout()->item(nx::Uuid(message[zoomUuidKey]));
            NX_VERBOSE(this, "RECEIVER: ZoomLinkAdded message %1 %2 %3",
                message, item, zoomTargetItem);
            if (!item || !zoomTargetItem)
                return;
            workbench()->currentLayout()->addZoomLink(item, zoomTargetItem);
            break;
        }
        case QnVideoWallControlMessage::ZoomLinkRemoved:
        {
            auto item = workbench()->currentLayout()->item(nx::Uuid(message[uuidKey]));
            auto zoomTargetItem = workbench()->currentLayout()->item(nx::Uuid(message[zoomUuidKey]));
            NX_VERBOSE(this, "RECEIVER: ZoomLinkRemoved message %1 %2 %3",
                message, item, zoomTargetItem);
            if (!item || !zoomTargetItem)
                return;
            workbench()->currentLayout()->removeZoomLink(item, zoomTargetItem);
            break;
        }
        case QnVideoWallControlMessage::NavigatorPositionChanged:
        {
            const qint64 position = QnLexical::deserialized<qint64>(message[kPositionUsecKey]);
            const bool silent = QnLexical::deserialized<bool>(message[kSilentKey]);
            navigator()->setPosition(position);
            if (!silent)
                menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
            break;
        }
        case QnVideoWallControlMessage::NavigatorPlayingChanged:
        {
            navigator()->setPlaying(QnLexical::deserialized<bool>(message[valueKey]));
            menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
            break;
        }
        case QnVideoWallControlMessage::NavigatorSpeedChanged:
        {
            navigator()->setSpeed(message[speedKey].toDouble());
            if (message.contains(kPositionUsecKey))
                navigator()->setPosition(message[kPositionUsecKey].toLongLong());
            menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
            break;
        }
        case QnVideoWallControlMessage::SynchronizationChanged:
        {
            const std::string value = message[valueKey].toStdString();
            StreamSynchronizationState state;
            const auto success = nx::reflect::json::deserialize(value, &state);
            if (NX_ASSERT(success))
            {
                workbench()->windowContext()->streamSynchronizer()->setState(state);
                menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
            }
            break;
        }
        case QnVideoWallControlMessage::MotionSelectionChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            auto regions = QJson::deserialized<MotionSelection>(value);
            auto widget = dynamic_cast<QnMediaResourceWidget*>(
                display()->widget(nx::Uuid(message[uuidKey])));
            if (widget)
                widget->setMotionSelection(regions);
            break;
        }
        case QnVideoWallControlMessage::MediaDewarpingParamsChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            auto params = QJson::deserialized<nx::vms::api::dewarping::MediaData>(value);
            auto widget = dynamic_cast<QnMediaResourceWidget*>(
                display()->widget(nx::Uuid(message[uuidKey])));
            if (widget)
                widget->setDewarpingParams(params);
            break;
        }
        case QnVideoWallControlMessage::Identify:
        {
            auto videoWall = resourcePool()->getResourceById<QnVideoWallResource>(
                m_videoWallMode.guid);
            if (!videoWall)
                return;

            QnVideoWallItem data = videoWall->items()->getItem(m_videoWallMode.instanceGuid);
            if (!data.name.isEmpty())
            {
                QFont font;
                font.setPixelSize(kIdentifyFontSize);
                font.setWeight(QFont::Light);
                SceneBanner::show(data.name, kIdentifyTimeout, font);
            }
            break;
        }
        case QnVideoWallControlMessage::RadassModeChanged:
        {
            int mode = message[valueKey].toInt();
            const auto items = QJson::deserialized<std::vector<nx::Uuid>>(message[itemsKey].toUtf8());

            const auto layout = workbench()->currentLayout()->resource();
            if (!layout)
                return;

            LayoutItemIndexList layoutItems;
            for (const auto& id: items)
                layoutItems.push_back(LayoutItemIndex(layout, id));

            menu::Parameters parameters(layoutItems);
            parameters.setArgument(Qn::ResolutionModeRole, mode);
            menu()->trigger(menu::RadassAction, parameters);
            break;
        }
        default:
            break;
    }
}

void QnWorkbenchVideoWallHandler::storeMessage(
    const QnVideoWallControlMessage& message,
    const nx::Uuid& controllerUuid,
    qint64 sequence)
{
    m_videoWallMode.storedMessages[controllerUuid][sequence] = message;
    NX_VERBOSE(this, "RECEIVER: store message %1", message);
}

void QnWorkbenchVideoWallHandler::restoreMessages(const nx::Uuid& controllerUuid, qint64 sequence)
{
    StoredMessagesHash &stored = m_videoWallMode.storedMessages[controllerUuid];
    while (stored.contains(sequence + 1))
    {
        QnVideoWallControlMessage message = stored.take(++sequence);
        NX_VERBOSE(this, "RECEIVER: restored message %1", message);
        handleMessage(message, controllerUuid, sequence);
    }
}

bool QnWorkbenchVideoWallHandler::canStartControlMode(const nx::Uuid& layoutId) const
{
    // There is no layout for the target item, so nobody already controls it.
    if (layoutId.isNull())
        return true;

    // Check if another client instance already controls this layout.
    const auto localInstanceId = runtimeInfoManager()->localInfo().uuid;
    const auto activePeers = runtimeInfoManager()->items()->getItems();
    for (const QnPeerRuntimeInfo& info: activePeers)
    {
        if (info.data.videoWallControlSession == layoutId && info.uuid != localInstanceId)
        {
            showControlledByAnotherUserMessage();
            return false;
        }
    }

    return true;
}

void QnWorkbenchVideoWallHandler::showControlledByAnotherUserMessage() const
{
    QnMessageBox::warning(mainWindowWidget(),
        tr("Screen is being controlled by another user"),
        tr("Control session cannot be started."));
}

void QnWorkbenchVideoWallHandler::showFailedToApplyChanges() const
{
    QnMessageBox::critical(mainWindowWidget(), tr("Failed to apply changes"));
}

QString QnWorkbenchVideoWallHandler::desktopCameraPhysicalId() const
{
    const auto user = context()->user();
    if (!user)
        return {};

    return core::DesktopResource::calculateUniqueId(peerId(), user->getId());
}

nx::Uuid QnWorkbenchVideoWallHandler::desktopCameraLayoutId() const
{
    return nx::Uuid::fromArbitraryData(desktopCameraPhysicalId() + "layout");
}

void QnWorkbenchVideoWallHandler::setControlMode(bool active)
{
    auto layoutResource = workbench()->currentLayout()->resource();
    NX_ASSERT(layoutResource || !active);

    if (active && !canStartControlMode(layoutResource->getId()))
        return;

    if (m_controlMode.active == active)
        return;

    m_controlMode.cachedMessages.clear();

    QnWorkbenchLayout* layout = workbench()->currentLayout();
    if (active)
    {
        connect(action(menu::RadassAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_radassAction_triggered);

        connect(workbench(), &Workbench::itemChanged,
            this, &QnWorkbenchVideoWallHandler::at_workbench_itemChanged);
        connect(layout, &QnWorkbenchLayout::itemAdded,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode);
        connect(layout, &QnWorkbenchLayout::itemRemoved,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode);
        connect(layout, &QnWorkbenchLayout::zoomLinkAdded,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded);
        connect(layout, &QnWorkbenchLayout::zoomLinkRemoved,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved);

        for (auto item: layout->items())
        {
            connect(item, &QnWorkbenchItem::dataChanged,
                this, &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);
        }

        m_controlMode.active = active;
        m_controlMode.cacheTimer->start();
        // TODO: #sivanov Start control when item goes online.
        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStarted));

        runtimeInfoManager()->updateLocalItem(
            [&layoutResource](auto* data)
            {
                data->data.videoWallControlSession = layoutResource->getId();
                return true;
            });
        QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();

        setItemControlledBy(localInfo.data.videoWallControlSession, localInfo.uuid, true);
    }
    else
    {
        action(menu::RadassAction)->disconnect(this);

        disconnect(workbench(), &Workbench::itemChanged,
            this, &QnWorkbenchVideoWallHandler::at_workbench_itemChanged);
        disconnect(layout, &QnWorkbenchLayout::itemAdded,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode);
        disconnect(layout, &QnWorkbenchLayout::itemRemoved,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode);
        disconnect(layout, &QnWorkbenchLayout::zoomLinkAdded,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded);
        disconnect(layout, &QnWorkbenchLayout::zoomLinkRemoved,
            this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved);

        for (auto item: layout->items())
        {
            disconnect(item, &QnWorkbenchItem::dataChanged,
                this, &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);
        }

        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStopped));
        m_controlMode.active = active;
        m_controlMode.cacheTimer->stop();

        runtimeInfoManager()->updateLocalItem(
            [](auto* data)
            {
                data->data.videoWallControlSession = nx::Uuid();
                return true;
            });
        QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();

        setItemControlledBy(localInfo.data.videoWallControlSession, localInfo.uuid, false);
    }
}

void QnWorkbenchVideoWallHandler::updateMode()
{
    QnWorkbenchLayout* layout = workbench()->currentLayout();

    nx::Uuid itemUuid = layout->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>();
    bool control = false;
    if (!itemUuid.isNull())
    {
        auto index = resourcePool()->getVideoWallItemByUuid(itemUuid);
        auto item = index.item();
        if (item.runtimeStatus.online
            && (item.runtimeStatus.controlledBy.isNull()
                || item.runtimeStatus.controlledBy == peerId())
            && layout->resource()
            && item.layout == layout->resource()->getId())
        {
            control = true;
        }
    }
    setControlMode(control);
}

void QnWorkbenchVideoWallHandler::submitDelayedItemOpen()
{
    // not logged in yet
    if (!m_videoWallMode.ready)
        return;

    // already opened or not submitted yet
    if (!m_videoWallMode.opening)
        return;

    m_videoWallMode.opening = false;

    auto videoWall = resourcePool()->getResourceById<QnVideoWallResource>(m_videoWallMode.guid);
    if (!videoWall || videoWall->items()->getItems().isEmpty())
    {
        QString message = videoWall
            ? "Warning: videowall %1 is empty, cannot start videowall on this pc"
            : "Warning: videowall %1 not exists, cannot start videowall on this pc";

        NX_ERROR(this, message.arg(m_videoWallMode.guid.toSimpleString()));

        VideoWallShortcutHelper::setVideoWallAutorunEnabled(m_videoWallMode.guid, false);
        closeInstanceDelayed();
        return;
    }

    nx::Uuid pcUuid = appContext()->localSettings()->pcUuid();
    if (pcUuid.isNull())
    {
        NX_ERROR(this, "Warning: pc UUID is null, cannot start videowall %1 on this pc",
            m_videoWallMode.guid);

        closeInstanceDelayed();
        return;
    }

    if (m_videoWallMode.instanceGuid.isNull()) //< Launcher mode.
        launchItems(videoWall);
    else
        openVideoWallItem(videoWall);
}

void QnWorkbenchVideoWallHandler::launchItems(const QnVideoWallResourcePtr& videoWall)
{
    using namespace nx::vms::applauncher::api;

    constexpr auto kCloseTimeout = 10s;

    const auto connection = this->connection();
    if (!NX_ASSERT(connection))
        return;

    const auto& moduleInformation = connection->moduleInformation();
    nx::utils::SoftwareVersion targetVersion;

    if (nx::vms::api::protocolVersion() != moduleInformation.protoVersion)
    {
        ClientVersionInfoList versions;
        const auto ret = getInstalledVersionsEx(&versions);

        targetVersion = latestVersionForProtocol(
            std::move(versions), moduleInformation.protoVersion);

        if (targetVersion.isNull())
        {
            NX_DEBUG(this, "No compatible client version %1, proto: %2, is installed: %3",
                moduleInformation.version, moduleInformation.protoVersion, ret);

            SceneBanner::show(
                tr("Cannot find compatible client version: %1")
                    .arg(moduleInformation.version.toString()),
                kCloseTimeout);

            executeDelayedParented(
                [this]() { closeInstanceDelayed(); }, kCloseTimeout, this);

            return;
        }
    }

    NX_DEBUG(this, "Launching child videowall items, version: %2", targetVersion);

    const auto pcUuid = appContext()->localSettings()->pcUuid();
    for (const auto& item: videoWall->items()->getItems())
    {
        if (item.pcUuid != pcUuid || item.runtimeStatus.online)
            continue;

        openNewWindow(m_videoWallMode.guid, item, targetVersion);
    }
    closeInstanceDelayed();
}

QnVideoWallItemIndexList QnWorkbenchVideoWallHandler::targetList() const
{
    auto layout = workbench()->currentLayout()->resource();
    if (!layout)
        return QnVideoWallItemIndexList();

    nx::Uuid currentId = layout->getId();

    QnVideoWallItemIndexList indices;

    for (const auto& videoWall: resourcePool()->getResources<QnVideoWallResource>())
    {
        for (const QnVideoWallItem &item : videoWall->items()->getItems())
        {
            if (item.layout == currentId && item.runtimeStatus.online)
                indices << QnVideoWallItemIndex(videoWall, item.uuid);
        }
    }

    return indices;
}

LayoutResourcePtr QnWorkbenchVideoWallHandler::constructLayout(
    const QnResourceList& resources) const
{
    QSet<QnResourcePtr> filtered;
    QnVirtualCameraResourceList cameras;
    QMap<qreal, int> aspectRatios;
    qreal defaultAr = QnLayoutResource::kDefaultCellAspectRatio;

    auto addToFiltered =
        [&](const QnResourcePtr &resource)
        {
            if (filtered.contains(resource))
                return;

            bool allowed = QnResourceAccessFilter::isShareableMedia(resource)
                || resource->hasFlags(Qn::desktop_camera)
                || resource->hasFlags(Qn::local_media);

            if (!allowed)
                return;

            filtered << resource;
            qreal ar = defaultAr;

            if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
            {
                cameras << camera;
                const auto cameraAr = camera->aspectRatio();
                ar = cameraAr.isValid() ? cameraAr.toFloat() : defaultAr;
            }
            aspectRatios[ar] = aspectRatios[ar] + 1;
        };

    for (const auto& resource : resources)
    {
        if (auto layout = resource.dynamicCast<LayoutResource>())
        {
            for (auto resource: layoutResources(layout))
                addToFiltered(resource);
        }
        else
        {
            addToFiltered(resource);
        }
    }

    /* If we have provided the only layout, copy from it as much as possible. */
    LayoutResourcePtr sourceLayout = resources.size() == 1
        ? resources.first().dynamicCast<LayoutResource>()
        : LayoutResourcePtr();

    LayoutResourcePtr layout = sourceLayout
        ? sourceLayout->clone()
        : LayoutResourcePtr(new LayoutResource());

    layout->setIdUnsafe(m_uuidPool->getFreeId());
    layout->addFlags(Qn::local);

    if (sourceLayout)
    {
        filterAllowedLayoutItems(layout);
    }
    else
    {
        qreal desiredAspectRatio = defaultAr;
        for (qreal ar : aspectRatios.keys())
        {
            if (aspectRatios[ar] > aspectRatios[desiredAspectRatio])
                desiredAspectRatio = ar;
        }

        layout->setCellSpacing(0);
        layout->setCellAspectRatio(desiredAspectRatio);

        /* Calculate size of the resulting matrix. */
        const int matrixWidth = qMax(1, qRound(std::sqrt(desiredAspectRatio * filtered.size())));

        int i = 0;
        for (const QnResourcePtr &resource: filtered)
        {
            LayoutItemData item = layoutItemFromResource(resource);
            item.flags = Qn::Pinned;
            item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
            layout->addItem(item);
            i++;
        }
    }

    return layout;
}

void QnWorkbenchVideoWallHandler::cleanupUnusedLayouts(const QnVideoWallResourceList& videowalls)
{
    LayoutResourceList layoutsToDelete;

    const auto videowallList = videowalls.empty()
        ? resourcePool()->getResources<QnVideoWallResource>()
        : videowalls;

    for (const auto& videowall: videowallList)
    {
        QSet<nx::Uuid> used;
        for (const QnVideoWallItem& item: videowall->items()->getItems())
            used << item.layout;

        for (const QnVideoWallMatrix& matrix: videowall->matrices()->getItems())
        {
            for (const auto& layoutId: matrix.layoutByItem.values())
                used << layoutId;
        }

        nx::Uuid videoWallId = videowall->getId();
        LayoutResourceList unused = resourcePool()->getResourcesByParentId(videoWallId)
            .filtered<LayoutResource>(
            [used](const LayoutResourcePtr& layout)
            {
                // this may be new just-set layout waiting for save
                if (layout->hasFlags(Qn::local))
                    return false;

                // Skip review layouts
                if (layout->data().contains(Qn::VideoWallResourceRole))
                    return false;

                return !used.contains(layout->getId());
            });
        layoutsToDelete.append(unused);
    }

    if (layoutsToDelete.isEmpty())
        return;

    // Deleting one-by-one to avoid invalid layouts which cannot be deleted for whatever reason
    for (auto layout: layoutsToDelete)
    {
        // If request was successfully sent, remove layout from the pool at once to make sure we
        // will not try to remove it again.
        if (menu()->triggerIfPossible(menu::RemoveFromServerAction, layout))
            resourcePool()->removeResource(layout);
    }
}

/*------------------------------------ HANDLERS ------------------------------------------*/

void QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered()
{
    if (nx::vms::common::saas::saasInitialized(systemContext()))
    {
        if (systemContext()->saasServiceManager()->saasShutDown())
        {
            const auto saasState = systemContext()->saasServiceManager()->saasState();

            const auto caption = tr("Site shut down");
            const auto text = tr("To add a Video Wall, the Site should be in active state. %1")
                .arg(nx::vms::common::saas::StringsHelper::recommendedAction(saasState)).trimmed();

            QnMessageBox::critical(mainWindowWidget(), caption, text);

            return;
        }
    }
    else if (m_licensesHelper->totalLicenses(Qn::LC_VideoWall) == 0)
    {
        showLicensesErrorDialog(
            tr("To enable this feature, please activate a Video Wall license."));
        return;
    }

    QStringList usedNames;
    for (const auto& resource: resourcePool()->getResourcesWithFlag(Qn::videowall))
        usedNames << resource->getName().trimmed().toLower();

    // TODO: #sivanov Refactor to the corresponding dialog.
    QString proposedName = nx::utils::generateUniqueString(
        usedNames,
        tr("Video Wall"),
        tr("Video Wall %1"));

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindowWidget()));
    dialog->setWindowTitle(tr("New Video Wall..."));
    dialog->setText(tr("Enter the name of Video Wall to create:"));
    dialog->setName(proposedName);
    dialog->setWindowModality(Qt::ApplicationModal);

    while (true)
    {
        if (!dialog->exec())
            return;

        proposedName = dialog->name().trimmed();
        if (proposedName.isEmpty())
            return;

        if (usedNames.contains(proposedName.toLower()))
        {
            messages::Videowall::anotherVideoWall(mainWindowWidget());
            continue;
        }

        break;
    };

    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setIdUnsafe(nx::Uuid::createUuid());
    videoWall->setName(proposedName);
    videoWall->setAutorun(true);
    resourcePool()->addResource(videoWall);

    auto callback =
        [this](bool success, const QnVideoWallResourcePtr& videoWall)
        {
            if (success)
            {
                menu()->trigger(menu::SelectNewItemAction, videoWall);
                menu()->trigger(menu::OpenVideoWallReviewAction, videoWall);
            }
            else
            {
                videoWall->resourcePool()->removeResource(videoWall);
            }
        };

    qnResourcesChangesManager->saveVideoWall(videoWall, callback);
}

void QnWorkbenchVideoWallHandler::at_deleteVideoWallAction_triggered()
{
    QnVideoWallResourceList videowalls = menu()->currentParameters(sender()).resources()
        .filtered<QnVideoWallResource>(
            [this](const QnResourcePtr& resource)
            {
                return menu()->canTrigger(menu::RemoveFromServerAction, resource);
            });

    if (videowalls.isEmpty())
        return;

    if (messages::Resources::deleteResources(mainWindowWidget(), videowalls))
    {
        VideoWallShortcutHelper videoWallShortcutHelper;
        for (const auto& videoWall: videowalls)
        {
            // Cleanup registry for the local pc. Remote PCs will cleanup autorun on first run.
            videoWallShortcutHelper.setVideoWallAutorunEnabled(videoWall->getId(), false);

            if (qnPlatform->shortcuts()->supported()
                && videoWallShortcutHelper.shortcutExists(videoWall))
            {
                videoWallShortcutHelper.deleteShortcut(videoWall);
            }

            qnResourcesChangesManager->deleteResource(videoWall);
        }
    }
}

void QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered()
{
    if (!context()->user())
        return;

    const auto parameters = menu()->currentParameters(sender());
    auto videoWall = parameters.resource().dynamicCast<QnVideoWallResource>();
    if (videoWall.isNull())
        return;

    auto dialog = createSelfDestructingDialog<QnAttachToVideowallDialog>(mainWindowWidget());
    dialog->loadFromResource(videoWall);

    connect(
        dialog,
        &QnAttachToVideowallDialog::accepted,
        this,
        [this, dialog, videoWall]
        {
            dialog->submitToResource(videoWall);
            qnResourcesChangesManager->saveVideoWall(videoWall);
            menu()->trigger(menu::OpenVideoWallReviewAction, videoWall);
        });

    dialog->show();
}

void QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered()
{
    const QnVideoWallItemIndexList items = menu()->currentParameters(sender()).videoWallItems();

    QSet<QnVideoWallResourcePtr> videoWalls;

    for (const QnVideoWallItemIndex& index: items)
    {
        if (!index.isValid())
            continue;

        QnVideoWallItem existingItem = index.item();
        if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(
            existingItem.layout))
        {
            // Check only Resources from the same System Context as the Layout. Cross-system
            // Resources cannot be available through Video Wall.
            QSet<nx::Uuid> resourceIds = layoutResourceIds(layout);
            auto removedResources = layout->resourcePool()->getResourcesByIds(resourceIds);
            if (!confirmRemoveResourcesFromLayout(layout, removedResources))
                break;
        }

        existingItem.layout = nx::Uuid();
        index.videowall()->items()->updateItem(existingItem);
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls, /*saveLayout*/ false,
        [this](bool successfullySaved, const QnVideoWallResourcePtr& videowall)
        {
            if (successfullySaved)
                cleanupUnusedLayouts({videowall});
        });
}

void QnWorkbenchVideoWallHandler::at_deleteVideoWallItemAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const QnVideoWallItemIndexList items = parameters.videoWallItems();

    if (!messages::Resources::deleteVideoWallItems(mainWindowWidget(), items))
        return;

    QSet<QnVideoWallResourcePtr> videoWalls;
    for (const auto& index: items)
    {
        if (!index.isValid())
            continue;

        index.videowall()->items()->removeItem(index.uuid());
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls, /*saveLayout*/ true,
        [this](bool successfullySaved, const QnVideoWallResourcePtr& videowall)
        {
            if (successfullySaved)
                cleanupUnusedLayouts({videowall});
        });
}

void QnWorkbenchVideoWallHandler::at_startVideoWallAction_triggered()
{
    auto videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (videoWall.isNull())
        return;

    switchToVideoWallMode(videoWall);
}

void QnWorkbenchVideoWallHandler::at_stopVideoWallAction_triggered()
{
    auto connection = messageBusConnection();
    if (!NX_ASSERT(connection))
        return;

    auto videoWallManager = connection->getVideowallManager(nx::network::rest::kSystemSession);

    auto videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (videoWall.isNull())
        return;

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Stop Video Wall?"),
        tr("To start it again, you should have physical access to its computer."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindowWidget());

    dialog.addButton(tr("Stop"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    nx::vms::api::VideowallControlMessageData message;
    message.operation = QnVideoWallControlMessage::Exit;
    message.videowallGuid = videoWall->getId();

    for (const QnVideoWallItem &item : videoWall->items()->getItems())
    {
        message.instanceGuid = item.uuid;
        videoWallManager->sendControlMessage(message, [](int /*requestId*/, ec2::ErrorCode) {});
    }
}

void QnWorkbenchVideoWallHandler::at_delayedOpenVideoWallItemAction_triggered()
{
    m_videoWallMode.guid =
        menu()->currentParameters(sender()).argument<nx::Uuid>(Qn::VideoWallGuidRole);
    m_videoWallMode.instanceGuid =
        menu()->currentParameters(sender()).argument<nx::Uuid>(Qn::VideoWallItemGuidRole);
    m_videoWallMode.opening = true;
    submitDelayedItemOpen();
}

void QnWorkbenchVideoWallHandler::at_renameAction_triggered()
{
    using NodeType = ResourceTree::NodeType;

    const auto parameters = menu()->currentParameters(sender());

    const auto nodeType = parameters.argument<NodeType>(Qn::NodeTypeRole, NodeType::resource);
    QString name = parameters.argument<QString>(core::ResourceNameRole).trimmed();

    bool valid = false;
    switch (nodeType)
    {
        case NodeType::videoWallItem:
            valid = parameters.videoWallItems().size() == 1
                && parameters.videoWallItems().first().isValid();
            break;
        case NodeType::videoWallMatrix:
            valid = parameters.videoWallMatrices().size() == 1
                && parameters.videoWallMatrices().first().isValid();
            break;
        default:
            // do nothing
            break;
    }

    NX_ASSERT(valid, "Data should correspond to action profile.");
    if (!valid)
        return;

    QString oldName;
    switch (nodeType)
    {
        case NodeType::videoWallItem:
            oldName = parameters.videoWallItems().first().item().name;
            break;
        case NodeType::videoWallMatrix:
            oldName = parameters.videoWallMatrices().first().matrix().name;
            break;
        default:
            // do nothing
            break;
    }

    if (name.isEmpty() || name == oldName)
        return;

    switch (nodeType)
    {
        case NodeType::videoWallItem:
        {
            QnVideoWallItemIndex index = parameters.videoWallItems().first();
            QnVideoWallItem existingItem = index.item();
            existingItem.name = name;
            index.videowall()->items()->updateItem(existingItem);
            saveVideowall(index.videowall());
        }
        break;

        case NodeType::videoWallMatrix:
        {
            QnVideoWallMatrixIndex index = parameters.videoWallMatrices().first();
            QnVideoWallMatrix existingMatrix = index.matrix();
            existingMatrix.name = name;
            index.videowall()->matrices()->updateItem(existingMatrix);
            saveVideowall(index.videowall());
        }
        break;

        default:
            // nothing
            break;
    }

}

void QnWorkbenchVideoWallHandler::at_identifyVideoWallAction_triggered()
{
    auto connection = messageBusConnection();
    if (!NX_ASSERT(connection))
        return;

    auto videoWallManager = connection->getVideowallManager(nx::network::rest::kSystemSession);

    const auto parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList indices = parameters.videoWallItems();
    if (indices.isEmpty())
    {
        for (auto videoWall: parameters.resources().filtered<QnVideoWallResource>())
        {
            for (const auto& item: videoWall->items()->getItems())
                indices << QnVideoWallItemIndex(videoWall, item.uuid);
        }
    }

    nx::vms::api::VideowallControlMessageData message;
    message.operation = QnVideoWallControlMessage::Identify;
    for (const QnVideoWallItemIndex &item : indices)
    {
        if (!item.isValid())
            continue;

        message.videowallGuid = item.videowall()->getId();
        message.instanceGuid = item.uuid();
        videoWallManager->sendControlMessage(message, [](int /*requestId*/, ec2::ErrorCode) {});
    }
}

void QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVideoWallItemIndex target;
    for (const QnVideoWallItemIndex& index: parameters.videoWallItems())
    {
        if (index.isValid())
        {
            target = index;
            break;
        }
    }

    if (!NX_ASSERT(target.isValid(), "Action was triggered with incorrect parameters"))
        return;

    QnVideoWallItem item = target.item();

    if (!canStartControlMode(item.layout))
        return;

    LayoutResourcePtr layoutResource = item.layout.isNull()
        ? LayoutResourcePtr()
        : resourcePool()->getResourceById<LayoutResource>(item.layout);

    if (!layoutResource)
    {
        layoutResource = constructLayout(QnResourceList());
        layoutResource->setParentId(target.videowall()->getId());
        layoutResource->setName(target.item().name);
        resourcePool()->addResource(layoutResource);
        resetLayout(QnVideoWallItemIndexList() << target, layoutResource);
    }

    auto layout = workbench()->layout(layoutResource);
    if (!layout)
        layout = workbench()->addLayout(layoutResource);

    layout->setData(Qn::VideoWallItemGuidRole, QVariant::fromValue(item.uuid));
    layout->notifyTitleChanged();

    if (layout)
        workbench()->setCurrentLayout(layout);
}

void QnWorkbenchVideoWallHandler::at_openVideoWallReviewAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto videoWall = parameters.resource().dynamicCast<QnVideoWallResource>();
    NX_ASSERT(videoWall);
    if (!videoWall)
        return;

    const auto existingLayout = QnWorkbenchLayout::instance(videoWall);
    if (existingLayout)
    {
        workbench()->setCurrentLayout(existingLayout);
        return;
    }

    /* Construct and add a new layout. */
    LayoutResourcePtr layout(new QnVideowallReviewLayoutResource(videoWall, context()));
    layout->setIdUnsafe(nx::Uuid::createUuid());
    if (accessController()->hasPermissions(videoWall, Qn::Permission::VideoWallLayoutPermissions))
        layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission));

    QMap<ScreenWidgetKey, QnVideoWallItemIndexList> itemGroups;

    for (const auto& item: videoWall->items()->getItems())
    {
        ScreenWidgetKey key(item.pcUuid, screensCoveredByItem(item, videoWall));
        itemGroups[key].append(QnVideoWallItemIndex(videoWall, item.uuid));
    }

    for (const auto& indices: itemGroups)
        addItemToLayout(layout, indices);

    menu()->trigger(menu::OpenInNewTabAction, layout);

    // New layout should not be marked as changed, make sure it will be done after layout opening.
    executeDelayedParented(
        [this, videoWall, layout]
        {
            saveVideowallAndReviewLayout(videoWall, layout);
        },
        this);
}

void QnWorkbenchVideoWallHandler::at_saveCurrentVideoWallReviewAction_triggered()
{
    auto layout = workbench()->currentLayoutResource();
    if (!NX_ASSERT(isVideoWallReviewLayout(layout)))
        return;

    auto videowall = layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!NX_ASSERT(videowall))
        return;

    saveVideowallAndReviewLayout(videowall, layout);
}

void QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto videowall = parameters.resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    saveVideowallAndReviewLayout(videowall);
}

void QnWorkbenchVideoWallHandler::at_dropOnVideoWallItemAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    nx::Uuid targetUuid = parameters.argument(Qn::VideoWallItemGuidRole).value<nx::Uuid>();
    QnVideoWallItemIndex targetIndex = resourcePool()->getVideoWallItemByUuid(targetUuid);
    if (!targetIndex.isValid())
        return;

    if (!ResourceAccessManager::hasPermissions(targetIndex.videowall(), Qn::ReadWriteSavePermission))
        return;

    // Layout that is currently on the drop-target item.
    auto currentLayout = resourcePool()->getResourceById<LayoutResource>(targetIndex.item().layout);

    if (currentLayout && currentLayout->locked())
    {
        SceneBanner::show(tr("Screen is locked and cannot be changed"));
        return;
    }

    auto keyboardModifiers = parameters.argument<Qt::KeyboardModifiers>(Qn::KeyboardModifiersRole);

    /* Case of dropping from one screen to another. */
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();
    QnVideoWallItemIndex sourceIndex;

    /* Resources that are being dropped (if dropped from tree or other screen). */
    QnResourceList targetResources;

    if (!videoWallItems.isEmpty())
    {
        for (const QnVideoWallItemIndex& index: videoWallItems)
        {
            if (!index.isValid())
                continue;

            if (auto layout = resourcePool()->getResourceById<LayoutResource>(index.item().layout))
                targetResources << layout;
        }

        // Dragging single videowall item causing swap (if Shift is not pressed).
        if (videoWallItems.size() == 1 && !videoWallItems.first().isNull()
            && !(Qt::ShiftModifier & keyboardModifiers))
        {
            sourceIndex = videoWallItems.first();
        }
    }
    else
    {
        targetResources = parameters.resources();
    }

    // Allow to drop a layout only if it's the only resource being dropped.
    // Otherwise layouts will be filtered out from the list.
    const bool layoutIsBeingDropped = targetResources.size() == 1
        && targetResources.front().objectCast<QnLayoutResource>();

    if (!layoutIsBeingDropped)
        filterAllowedMediaResources(targetResources);

    if (targetResources.isEmpty())
        return;

    enum class Action
    {
        NoAction,
        AddAction,
        SwapAction,
        SetAction
    };
    Action dropAction = Action::NoAction;

    bool hasDesktopCamera = std::any_of(targetResources.begin(), targetResources.end(),
        [](const QnResourcePtr& resource) { return resource->hasFlags(Qn::desktop_camera); });

    if (currentLayout)
    {
        const auto& values = currentLayout->getItems().values();
        hasDesktopCamera |= std::any_of(values.begin(), values.end(),
            [this](const LayoutItemData &item)
            {
                QnResourcePtr childResource = resourcePool()->getResourceById(item.resource.id);
                return childResource && childResource->hasFlags(Qn::desktop_camera);
            });
    }

    /* If Control pressed, add items to current layout. */
    if (Qt::ControlModifier & keyboardModifiers && currentLayout && !hasDesktopCamera)
    {
        targetResources << currentLayout;
        dropAction = Action::AddAction;
    }
    /* Check if we are dragging from another screen. */
    else if (!sourceIndex.isNull())
    {
        dropAction = Action::SwapAction;
    }
    else
    {
        dropAction = Action::SetAction;
    }

    nx::Uuid videoWallId = targetIndex.videowall()->getId();

    LayoutResourcePtr targetLayout;
    if (targetResources.size() == 1)
    {
        auto layout = targetResources.first().dynamicCast<LayoutResource>();
        if (layout)
        {
            if (layout->getParentId() == videoWallId || layout->hasFlags(Qn::exported_layout))
                targetLayout = layout;
        }
    }

    if (!targetLayout)
    {
        targetLayout = constructLayout(targetResources);
        targetLayout->setName(targetIndex.item().name);
        targetLayout->setParentId(videoWallId);
    }

    // User can occasionally remove own access to cameras by dropping something on videowall.
    if (dropAction == Action::SetAction && currentLayout)
    {
        // Check only Resources from the same System Context as the Layout. Cross-system Resources
        // cannot be available through Video Wall.
        const auto oldResources = layoutResourceIds(currentLayout);
        const auto newResources = layoutResourceIds(targetLayout);

        const auto removedResources = resourcePool()->getResourcesByIds(oldResources - newResources);
        if (!confirmRemoveResourcesFromLayout(currentLayout, removedResources))
            return;
    }

    if (!targetLayout->resourcePool())
        resourcePool()->addResource(targetLayout);

    switch (dropAction)
    {
        case Action::AddAction:
        case Action::SetAction:
            if (checkLocalFiles(targetIndex, targetLayout))
                resetLayout(QnVideoWallItemIndexList() << targetIndex, targetLayout);
            return; //< No need to call cleanupUnusedLayouts() as `resetLayout()` does it.
        case Action::SwapAction:
            if (checkLocalFiles(targetIndex, targetLayout) && checkLocalFiles(sourceIndex, currentLayout))
                swapLayouts(targetIndex, targetLayout, sourceIndex, currentLayout);
            break;
        default:
            break;
    }
    cleanupUnusedLayouts();
}

void QnWorkbenchVideoWallHandler::at_pushMyScreenToVideowallAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();

    for (const QnVideoWallItemIndex& videoWallScreenIndex: videoWallItems)
    {
        if (!videoWallScreenIndex.isValid())
            continue;

        if (!ResourceAccessManager::hasPermissions(
            videoWallScreenIndex.videowall(), Qn::ReadWriteSavePermission))
        {
            continue;
        }

        // Layout that is currently on the drop-target item.
        auto currentLayout = resourcePool()->getResourceById<LayoutResource>(
            videoWallScreenIndex.item().layout);

        if (currentLayout && currentLayout->locked())
        {
            SceneBanner::show(tr("Screen is locked and cannot be changed"));
            continue;
        }

        const auto desktopCamera =
            resourcePool()->getResourceByPhysicalId<QnVirtualCameraResource>(
                desktopCameraPhysicalId());

        LayoutResourcePtr targetLayout;
        if (desktopCamera
            && desktopCamera->hasFlags(Qn::desktop_camera)
            && !desktopCamera->hasFlags(Qn::fake))
        {
            m_localDesktopCamera = desktopCamera;
            targetLayout = constructLayout({desktopCamera});

            NX_INFO(this, "Push my screen is handled. Already existing desktop camera is used.");
        }
        else
        {
            const auto localCameraStub =
                systemContext()->desktopCameraStubController()->createLocalCameraStub(
                    desktopCameraPhysicalId());

            m_localDesktopCamera = localCameraStub;
            targetLayout = constructLayout({localCameraStub});

            NX_INFO(this, "Push my screen is handled. New desktop camera is constructed.");
        }

        m_videoWallsWithLocalDesktopCamera << videoWallScreenIndex.videowall()->getId();
        m_localDesktopCameraLayoutIds << targetLayout->getId();

        targetLayout->setParentId(videoWallScreenIndex.videowall()->getId());
        targetLayout->setName(videoWallScreenIndex.item().name);
        resourcePool()->addResource(targetLayout);
        resetLayout(QnVideoWallItemIndexList() << videoWallScreenIndex, targetLayout);
    }

    systemContext()->desktopCameraConnectionController()->initialize();
}

void QnWorkbenchVideoWallHandler::at_videowallSettingsAction_triggered()
{
    auto videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    auto dialog = createSelfDestructingDialog<QnVideowallSettingsDialog>(mainWindowWidget());
    dialog->loadFromResource(videowall);

    connect(
        dialog,
        &QnVideowallSettingsDialog::accepted,
        this,
        [this, dialog, videowall]
        {
            dialog->submitToResource(videowall);
            saveVideowall(videowall);
        });

    dialog->show();
}

void QnWorkbenchVideoWallHandler::at_saveVideowallMatrixAction_triggered()
{
    auto videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    QnVideoWallMatrix matrix;
    matrix.name = tr("New Matrix %1").arg(videowall->matrices()->getItems().size() + 1);
    matrix.uuid = nx::Uuid::createUuid();

    foreach(const QnVideoWallItem &item, videowall->items()->getItems())
    {
        if (item.layout.isNull() || !resourcePool()->getResourceById(item.layout))
            continue;
        matrix.layoutByItem[item.uuid] = item.layout;
    }

    if (matrix.layoutByItem.isEmpty())
    {
        QnMessageBox::warning(mainWindowWidget(), tr("Cannot save empty matrix"));
        return;
    }

    videowall->matrices()->addItem(matrix);
    saveVideowall(videowall);
}

void QnWorkbenchVideoWallHandler::at_loadVideowallMatrixAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    const QnVideoWallMatrixIndexList matrices = parameters.videoWallMatrices();
    if (matrices.size() != 1)
        return;

    const QnVideoWallMatrixIndex index = matrices.first();
    if (!index.isValid())
        return;

    const QnWorkbenchLayoutsChangeValidator validator(context());
    if (!validator.confirmLoadVideoWallMatrix(index))
        return;

    const auto videowall = index.videowall();
    const auto matrix = index.matrix();

    bool hasChanges = false;
    for (auto item: videowall->items()->getItems())
    {
        if (!matrix.layoutByItem.contains(item.uuid))
            continue;

        nx::Uuid layoutUuid = matrix.layoutByItem[item.uuid];
        if (!layoutUuid.isNull() && !resourcePool()->getResourceById<QnLayoutResource>(layoutUuid))
            layoutUuid = nx::Uuid();

        if (item.layout == layoutUuid)
            continue;

        item.layout = layoutUuid;
        videowall->items()->updateItem(item);
        hasChanges = true;
    }

    if (!hasChanges)
        return;

    saveVideowall(videowall);
}

void QnWorkbenchVideoWallHandler::at_deleteVideowallMatrixAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const QnVideoWallMatrixIndexList matrices = parameters.videoWallMatrices();

    if (!messages::Resources::deleteVideoWallMatrices(mainWindowWidget(), matrices))
        return;

    const QnWorkbenchLayoutsChangeValidator validator(context());
    if (!validator.confirmDeleteVideoWallMatrices(matrices))
        return;

    QSet<QnVideoWallResourcePtr> videoWalls;
    for (const auto& matrix: matrices)
    {
        if (!matrix.videowall())
            continue;

        matrix.videowall()->matrices()->removeItem(matrix.uuid());
        videoWalls << matrix.videowall();
    }

    saveVideowalls(videoWalls);
}

void QnWorkbenchVideoWallHandler::at_videoWallScreenSettingsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();
    if (videoWallItems.empty())
        return;

    const auto& index = videoWallItems.first();
    auto layout = resourcePool()->getResourceById<LayoutResource>(index.item().layout);
    if (!layout)
    {
        layout = constructLayout(QnResourceList());
        layout->setParentId(index.videowall()->getId());
        layout->setName(index.item().name);
        resourcePool()->addResource(layout);
        resetLayout(QnVideoWallItemIndexList() << index, layout);
    }

    menu()->trigger(menu::LayoutSettingsAction, layout);
}

void QnWorkbenchVideoWallHandler::at_radassAction_triggered()
{
    if (!m_controlMode.active)
        return;

    const auto parameters = menu()->currentParameters(sender());

    const auto mode = parameters.argument(Qn::ResolutionModeRole).toInt();

    std::vector<nx::Uuid> items;
    for (const auto& item: parameters.layoutItems())
        items.push_back(item.uuid());

    QnVideoWallControlMessage message(QnVideoWallControlMessage::RadassModeChanged);
    message[valueKey] = QString::number(mode);
    message[itemsKey] = QString::fromUtf8(QJson::serialized(items));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_resPool_resourceAdded(const QnResourcePtr& resource)
{
    /* Exclude from pool all existing resources ids. */
    m_uuidPool->markAsUsed(resource->getId());

    auto videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (!videoWall)
        return;

    auto handleAutoRunChanged =
        [](const QnVideoWallResourcePtr& videoWall)
        {
            if (videoWall && videoWall->pcs()->hasItem(appContext()->localSettings()->pcUuid()))
            {
                VideoWallShortcutHelper::setVideoWallAutorunEnabled(
                    videoWall->getId(),
                    videoWall->isAutorun(),
                    serverUrl(videoWall));
            }
        };
    handleAutoRunChanged(videoWall);

    connect(videoWall.get(), &QnVideoWallResource::autorunChanged, this, handleAutoRunChanged);

    connect(videoWall.get(), &QnVideoWallResource::pcAdded, this,
        [](const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
        {
            if (pc.uuid != appContext()->localSettings()->pcUuid())
                return;

            VideoWallShortcutHelper::setVideoWallAutorunEnabled(
                videoWall->getId(),
                videoWall->isAutorun(),
                serverUrl(videoWall));
        });

    connect(videoWall.get(), &QnVideoWallResource::pcRemoved, this,
        [](const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
        {
            if (pc.uuid != appContext()->localSettings()->pcUuid())
                return;
            VideoWallShortcutHelper::setVideoWallAutorunEnabled(videoWall->getId(), false);
        });

    if (m_videoWallMode.active)
    {
        if (videoWall->getId() == m_videoWallMode.guid)
        {
            connect(videoWall.get(), &QnVideoWallResource::itemChanged,
                this, &QnWorkbenchVideoWallHandler::at_videoWall_itemChanged_activeMode);
            connect(videoWall.get(), &QnVideoWallResource::itemRemoved,
                this, &QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode);
            VideoWallShortcutHelper::setVideoWallAutorunEnabled(
                videoWall->getId(),
                videoWall->isAutorun(),
                serverUrl(videoWall));

            auto handleTimelineEnabledChanged =
                [this](const QnVideoWallResourcePtr& videoWall)
                {
                    qnRuntime->setVideoWallWithTimeLine(videoWall->isTimelineEnabled());
                    menu()->triggerIfPossible(menu::ShowTimeLineOnVideowallAction);
                };

            connect(videoWall.get(), &QnVideoWallResource::timelineEnabledChanged, this,
                handleTimelineEnabledChanged);

            handleTimelineEnabledChanged(videoWall);

            if (m_videoWallMode.ready)
                openVideoWallItem(videoWall);
        }
    }
    else
    {
        connect(videoWall.get(), &QnVideoWallResource::pcAdded,
            this, &QnWorkbenchVideoWallHandler::at_videoWall_pcAdded);
        connect(videoWall.get(), &QnVideoWallResource::pcChanged,
            this, &QnWorkbenchVideoWallHandler::at_videoWall_pcChanged);
        connect(videoWall.get(), &QnVideoWallResource::pcRemoved,
            this, &QnWorkbenchVideoWallHandler::at_videoWall_pcRemoved);
        connect(videoWall.get(), &QnVideoWallResource::itemAdded,
            this, &QnWorkbenchVideoWallHandler::at_videoWall_itemAdded);
        connect(videoWall.get(), &QnVideoWallResource::itemChanged,
            this, &QnWorkbenchVideoWallHandler::at_videoWall_itemChanged);
        connect(videoWall.get(), &QnVideoWallResource::itemRemoved,
            this, &QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved);

        for (const auto& item: videoWall->items()->getItems())
            at_videoWall_itemAdded(videoWall, item);
    }
}

void QnWorkbenchVideoWallHandler::at_resPool_resourceRemoved(const QnResourcePtr &resource)
{
    /* Return id to the pool. */
    m_uuidPool->markAsFree(resource->getId());

    if (resource == m_localDesktopCamera)
    {
        m_localDesktopCamera.reset();
        return;
    }

    QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (!videoWall)
        return;

    disconnect(videoWall.get(), nullptr, this, nullptr);
    if (m_videoWallMode.active)
    {
        if (resource->getId() != m_videoWallMode.guid)
            return;
        closeInstanceDelayed();
    }
    else
    {
        QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
        if (layout && layout->resource())
            resourcePool()->removeResource(layout->resource());
    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcAdded(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallPcData& pc)
{
    auto layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    for (const QnVideoWallItem& item: videoWall->items()->getItems())
    {
        if (item.pcUuid != pc.uuid)
            continue;
        at_videoWall_itemAdded(videoWall, item);
    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcChanged(
    [[maybe_unused]] const QnVideoWallResourcePtr& videoWall,
    [[maybe_unused]] const QnVideoWallPcData& pc)
{
    // TODO: #sivanov Implement screen size changes handling.
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcRemoved(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallPcData& pc)
{
    auto layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    QList<QnWorkbenchItem*> itemsToDelete;
    for (auto workbenchItem: layout->items())
    {
        LayoutItemData data = workbenchItem->data();
        auto indices = getIndices(layout->resource(), data);
        if (indices.isEmpty())
            continue;
        if (indices.first().item().pcUuid != pc.uuid)
            continue;

        itemsToDelete << workbenchItem;
    }

    // TODO: #sivanov Check if the item is actually destroyed.
    for (auto workbenchItem: itemsToDelete)
        layout->removeItem(workbenchItem);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemAdded(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& item)
{
    QnVideoWallItem updatedItem(item);
    bool hasChanges = false;

    for (const auto& info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.videoWallInstanceGuid == item.uuid)
        {
            hasChanges |= !updatedItem.runtimeStatus.online;
            updatedItem.runtimeStatus.online = true;
        }

        if (info.data.videoWallControlSession == item.layout
            && !info.data.videoWallControlSession.isNull())
        {
            hasChanges |= updatedItem.runtimeStatus.controlledBy != info.uuid;
            updatedItem.runtimeStatus.controlledBy = info.uuid;
        }
    }

    updateReviewLayout(videoWall, item, ItemAction::Added);

    if (hasChanges)
    {
        videoWall->items()->updateItem(updatedItem);
        updateMode();
    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& item)
{
    // TODO: #sivanov Implement screen size changes handling.

    /* Check if item's layout was changed. */
    nx::Uuid controller = getLayoutController(item.layout);
    if (item.runtimeStatus.controlledBy != controller)
    {
        QnVideoWallItem updatedItem(item);
        updatedItem.runtimeStatus.controlledBy = controller;
        videoWall->items()->updateItem(updatedItem);
        return; // we will re-enter the handler
    }

    updateControlLayout(item, ItemAction::Changed);
    updateReviewLayout(videoWall, item, ItemAction::Changed);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& item)
{
    updateControlLayout(item, ItemAction::Removed);
    updateReviewLayout(videoWall, item, ItemAction::Removed);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged_activeMode(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& item)
{
    if (videoWall->getId() != m_videoWallMode.guid || item.uuid != m_videoWallMode.instanceGuid)
        return;

    openVideoWallItem(videoWall);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& item)
{
    if (videoWall->getId() != m_videoWallMode.guid || item.uuid != m_videoWallMode.instanceGuid)
        return;

    closeInstanceDelayed();
}

void QnWorkbenchVideoWallHandler::at_eventManager_controlMessageReceived(
    const nx::vms::api::VideowallControlMessageData& apiMessage)
{
    if (apiMessage.instanceGuid != m_videoWallMode.instanceGuid)
        return;

    QnVideoWallControlMessage message;
    ec2::fromApiToResource(apiMessage, message);

    // Ignore order for broadcast messages such as Exit or Identify
    if (!message.params.contains(sequenceKey))
    {
        handleMessage(message);
        return;
    }

    nx::Uuid controllerUuid = nx::Uuid(message[pcUuidKey]);
    qint64 sequence = message[sequenceKey].toULongLong();

    // ControlStarted message set starting sequence number
    if (message.operation == QnVideoWallControlMessage::ControlStarted)
    {
        handleMessage(message, controllerUuid, sequence);
        restoreMessages(controllerUuid, sequence);
        return;
    }

    // all messages should go one-by-one
    // TODO: #sivanov What if one message is lost forever? Timeout?
    if (!m_videoWallMode.sequenceByPcUuid.contains(controllerUuid) ||
        (sequence - m_videoWallMode.sequenceByPcUuid[controllerUuid] > 1))
    {
        if (!m_videoWallMode.sequenceByPcUuid.contains(controllerUuid))
        {
            NX_VERBOSE(this,
                "RECEIVER: first message from this controller, waiting for ControlStarted");
        }
        else
        {
            NX_VERBOSE(this, "RECEIVER: current sequence %1",
                m_videoWallMode.sequenceByPcUuid[controllerUuid]);
        }
        storeMessage(message, controllerUuid, sequence);
        return;
    }

    // skip outdated messages and hope for the best
    if (sequence < m_videoWallMode.sequenceByPcUuid[controllerUuid])
    {
        NX_ASSERT(false, "outdated control message");
        return;
    }

    // Correct ordered message
    handleMessage(message, controllerUuid, sequence);

    //check for messages with next sequence
    restoreMessages(controllerUuid, sequence);
    }

void QnWorkbenchVideoWallHandler::at_display_widgetAdded(QnResourceWidget* widget)
{
    if (widget->resource()->flags().testFlag(Qn::sync))
    {
        if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
        {
            connect(mediaWidget, &QnMediaResourceWidget::motionSelectionChanged,
                this, &QnWorkbenchVideoWallHandler::at_widget_motionSelectionChanged);
            connect(mediaWidget, &QnMediaResourceWidget::dewarpingParamsChanged,
                this, &QnWorkbenchVideoWallHandler::at_widget_dewarpingParamsChanged);
        }
    }
}

void QnWorkbenchVideoWallHandler::at_display_widgetAboutToBeRemoved(QnResourceWidget* widget)
{
    widget->disconnect(this);
}

void QnWorkbenchVideoWallHandler::at_widget_motionSelectionChanged()
{
    auto widget = checked_cast<QnMediaResourceWidget*>(sender());
    if (!widget)
        return;

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::MotionSelectionChanged);
    message[uuidKey] = widget->item()->uuid().toString(QUuid::WithBraces);
    message[valueKey] = QString::fromUtf8(QJson::serialized<MotionSelection>(
        widget->motionSelection()));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_widget_dewarpingParamsChanged()
{
    QnMediaResourceWidget* widget = checked_cast<QnMediaResourceWidget *>(sender());
    if (!widget)
        return;

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::MediaDewarpingParamsChanged);
    message[uuidKey] = widget->item()->uuid().toString(QUuid::WithBraces);
    message[valueKey] = QString::fromUtf8(QJson::serialized(widget->dewarpingParams()));
    sendMessage(message, /*cached*/ true);
}

void QnWorkbenchVideoWallHandler::at_workbench_currentLayoutAboutToBeChanged()
{
    workbench()->currentLayout()->disconnect(this);
    workbench()->currentLayoutResource()->disconnect(this);
    setControlMode(false);
}

void QnWorkbenchVideoWallHandler::at_workbench_currentLayoutChanged()
{
    connect(workbench()->currentLayout(), &QnWorkbenchLayout::dataChanged,
        this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_dataChanged);

    auto layout = workbench()->currentLayoutResource();

    connect(layout.get(), &QnLayoutResource::cellSpacingChanged, this,
        [this]()
        {
            if (!m_controlMode.active)
                return;

            QByteArray json;
            qreal value = workbench()->currentLayoutResource()->cellSpacing();
            QJson::serialize(value, &json);

            QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutDataChanged);
            message[roleKey] = QString::number(Qn::LayoutCellSpacingRole);
            message[valueKey] = QString::fromUtf8(json);
            sendMessage(message);
        });

    connect(layout.get(), &QnLayoutResource::cellAspectRatioChanged, this,
        [this]()
        {
            if (!m_controlMode.active)
                return;

            QByteArray json;
            qreal value = workbench()->currentLayoutResource()->cellSpacing();
            QJson::serialize(value, &json);

            QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutDataChanged);
            message[roleKey] = QString::number(Qn::LayoutCellAspectRatioRole);
            message[valueKey] = QString::fromUtf8(json);
            sendMessage(message);
        });

    updateMode();
}

void QnWorkbenchVideoWallHandler::at_workbench_itemChanged(Qn::ItemRole role)
{
    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::ItemRoleChanged);
    message[roleKey] = QString::number(role);
    message[uuidKey] = workbench()->item(role)
        ? workbench()->item(role)->uuid().toString(QUuid::WithBraces)
        : QString();
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *item)
{
    connect(item, &QnWorkbenchItem::dataChanged, this,
        &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);

    if (!m_controlMode.active)
        return;

    const auto itemUuid = item->uuid().toString(QUuid::WithBraces);

    {
        QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemAdded);
        message[uuidKey] = itemUuid;
        message[resourceKey] = item->resource()->getId().toSimpleString();
        message[geometryKey] = QString::fromUtf8(QJson::serialized(item->geometry()));
        message[zoomRectKey] = QString::fromUtf8(QJson::serialized(item->zoomRect()));
        message[rotationKey] = QString::fromUtf8(QJson::serialized(item->rotation()));
        message[checkedButtonsKey] = QString::fromUtf8(QJson::serialized(
            item->data(Qn::ItemCheckedButtonsRole).toInt()));
        sendMessage(message);
    }

    if (const auto mediaResource = item->resource().dynamicCast<QnMediaResource>())
    {
        QnVideoWallControlMessage message(QnVideoWallControlMessage::MediaDewarpingParamsChanged);
        message[uuidKey] = itemUuid;
        message[valueKey] = QString::fromUtf8(QJson::serialized(mediaResource->getDewarpingParams()));
        sendMessage(message, /*cached*/ true);
    }

    const auto itemDewarpingData = item->data(Qn::ItemImageDewarpingRole)
        .value<nx::vms::api::dewarping::ViewData>();
    if (itemDewarpingData.enabled)
    {
        QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemDataChanged);
        message[roleKey] = QString::number(Qn::ItemImageDewarpingRole);
        message[uuidKey] = itemUuid;
        message[valueKey] = QString::fromUtf8(QJson::serialized(itemDewarpingData));
        sendMessage(message, /*cached*/ true);
    }
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem* item)
{
    disconnect(item, &QnWorkbenchItem::dataChanged, this,
        &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemRemoved);
    message[uuidKey] = item->uuid().toString(QUuid::WithBraces);
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded(
    QnWorkbenchItem* item,
    QnWorkbenchItem* zoomTargetItem)
{
    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::ZoomLinkAdded);
    message[uuidKey] = item->uuid().toString(QUuid::WithBraces);
    message[zoomUuidKey] = zoomTargetItem->uuid().toString(QUuid::WithBraces);
    sendMessage(message);
    NX_VERBOSE(this, "SENDER: zoom Link added %1 %2", item->uuid(), zoomTargetItem->uuid());
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved(
    QnWorkbenchItem* item,
    QnWorkbenchItem* zoomTargetItem)
{
    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::ZoomLinkRemoved);
    message[uuidKey] = item->uuid().toString(QUuid::WithBraces);
    message[zoomUuidKey] = zoomTargetItem->uuid().toString(QUuid::WithBraces);
    sendMessage(message);
    NX_VERBOSE(this, "SENDER: zoom Link removed %1 %2", item->uuid(), zoomTargetItem->uuid());
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_dataChanged(int role)
{
    if (role == Qn::VideoWallItemGuidRole || role == Qn::VideoWallResourceRole)
    {
        updateMode();
        return;
    }
}

// Keep in sync with QnWorkbenchVideoWallHandler::handleMessage.
void QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged(int role)
{
    if (!m_controlMode.active)
        return;

    static const QSet<int> kIgnoreOnLayoutChange =
        {Qn::ItemPausedRole, Qn::ItemSpeedRole, Qn::ItemPositionRole};

    if (display()->isChangingLayout() && kIgnoreOnLayoutChange.contains(role))
        return;

    const QnWorkbenchItem* item = static_cast<QnWorkbenchItem*>(sender());
    QByteArray json;
    QVariant data = item->data(role);
    bool cached = false;

    switch (role)
    {
        // Do not transfer flags, it may result of incorrect pending geometry adjustment.
        case Qn::ItemFlagsRole:

        // Do not transfer static time, it may result of pause.
        case Qn::ItemTimeRole:

        // Do not transfer calculated geometry.
        case Qn::ItemCombinedGeometryRole:
        case Qn::ItemGeometryDeltaRole:
            return;

        case Qn::ItemPositionRole:
        {
            const auto value = data.value<QPointF>();
            NX_VERBOSE(this, "SENDER: Item %1 %2 changed to %3",
                item->uuid(), QnLexical::serialized(role), value);
            QJson::serialize(value, &json);
            break;
        }

        case Qn::ItemGeometryRole:
        {
            QRect value = data.value<QRect>();
            NX_VERBOSE(this, "SENDER: Item %1 %2 changed to %3",
                item->uuid(), QnLexical::serialized(role), value);
            QJson::serialize(value, &json);
            break;
        }

        case core::ItemZoomRectRole:
        {
            QRectF value = data.value<QRectF>();
            NX_VERBOSE(this, "SENDER: Item %1 %2 changed to %3",
                item->uuid(), QnLexical::serialized(role), value);
            QJson::serialize(value, &json);
            cached = true;
            break;
        }

        case Qn::ItemPausedRole:
        case Qn::ItemZoomWindowRectangleVisibleRole:
        case Qn::ItemFlipRole:
        {
            const auto value = data.toBool();
            NX_VERBOSE(this, "SENDER: Item %1 %2 changed to %3",
                item->uuid(), QnLexical::serialized(role), value);
            QJson::serialize(value, &json);
            break;
        }

        case Qn::ItemSpeedRole:
        case Qn::ItemRotationRole:
        {
            qreal value = data.toReal();
            NX_VERBOSE(this, "SENDER: Item %1 %2 changed to %3",
                item->uuid(), QnLexical::serialized(role), value);
            QJson::serialize(value, &json);
            break;
        }
        case Qn::ItemCheckedButtonsRole:
        {
            int value = data.toInt();
            NX_VERBOSE(this, "SENDER: Item %1 %2 changed to %3",
                item->uuid(), QnLexical::serialized(role), value);
            QJson::serialize(value, &json);
            break;
        }
        case Qn::ItemFrameDistinctionColorRole:
        {
            QColor value = data.value<QColor>();
            NX_VERBOSE(this, "SENDER: Item %1 %2 changed to %3",
                item->uuid(), QnLexical::serialized(role), value);
            json = QByteArray::fromStdString(nx::reflect::toString(value));
            break;
        }

        case Qn::ItemSliderWindowRole:
        case Qn::ItemSliderSelectionRole:
            QJson::serialize(data.value<QnTimePeriod>(), &json);
            break;

        case Qn::ItemImageEnhancementRole:
            QJson::serialize(data.value<nx::vms::api::ImageCorrectionData>(), &json);
            break;

        case Qn::ItemImageDewarpingRole:
            QJson::serialize(data.value<nx::vms::api::dewarping::ViewData>(), &json);
            cached = true;
            break;

        case Qn::ItemHealthMonitoringButtonsRole:
            QJson::serialize(data.value<QnServerResourceWidget::HealthMonitoringButtons>(), &json);
            break;

        default:
            NX_VERBOSE(this, "SENDER: Skipping role %1", QnLexical::serialized(role));
            break;
        }

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemDataChanged);
    message[roleKey] = QString::number(role);
    message[uuidKey] = item->uuid().toString(QUuid::WithBraces);
    message[valueKey] = QString::fromUtf8(json);
    sendMessage(message, cached);
}

void QnWorkbenchVideoWallHandler::syncTimelinePosition(bool silent)
{
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    if (navigator()->positionUsec() == qint64(AV_NOPTS_VALUE))
        return;

    // No need to send playback command for non-playable widgets.
    if (!navigator()->currentMediaWidget())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::NavigatorPositionChanged);
    message[kPositionUsecKey] = QnLexical::serialized(navigator()->positionUsec());
    message[kSilentKey] = QnLexical::serialized(silent);
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_navigator_playingChanged()
{
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    // No need to send playback command for non-playable widgets.
    if (!navigator()->currentMediaWidget())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::NavigatorPlayingChanged);
    message[valueKey] = QnLexical::serialized(navigator()->isPlaying());
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_navigator_speedChanged()
{
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    // No need to send playback command for non-playable widgets.
    if (!navigator()->currentMediaWidget())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::NavigatorSpeedChanged);
    message[speedKey] = QString::number(navigator()->speed());
    auto position = navigator()->positionUsec();
    if (position != AV_NOPTS_VALUE)
        message[kPositionUsecKey] = QString::number(position);
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchStreamSynchronizer_runningChanged()
{
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::SynchronizationChanged);
    message[valueKey] = QString::fromStdString(nx::reflect::json::serialize(
        workbench()->windowContext()->streamSynchronizer()->state()));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_controlModeCacheTimer_timeout()
{
    if (m_controlMode.cachedMessages.isEmpty())
        return;

    while (!m_controlMode.cachedMessages.isEmpty())
    {
        QnVideoWallControlMessage message = m_controlMode.cachedMessages.takeLast();
        for (auto iter = m_controlMode.cachedMessages.begin();
            iter != m_controlMode.cachedMessages.end();)
        {
            if ((iter->operation == message.operation)
                && (iter->params[roleKey] == message[roleKey])
                && (iter->params[uuidKey] == message[uuidKey]))
            {
                iter = m_controlMode.cachedMessages.erase(iter);
            }
            else
            {
                iter++;
            }
        }
        sendMessage(message);
    }
}

void QnWorkbenchVideoWallHandler::saveVideowall(
    const QnVideoWallResourcePtr& videowall,
    bool saveLayout,
    VideoWallCallbackFunction callback)
{
    if (!ResourceAccessManager::hasPermissions(videowall, Qn::ReadWriteSavePermission))
    {
        if (callback)
        {
            // Call the callback asynchronously, for uniformity with other places.
            executeLater([=]() { callback(/*success*/ false, videowall); }, this);
        }
        return;
    }

    if (saveLayout && QnWorkbenchLayout::instance(videowall))
        saveVideowallAndReviewLayout(videowall, {}, callback);
    else
        qnResourcesChangesManager->saveVideoWall(videowall, callback);
}

void QnWorkbenchVideoWallHandler::saveVideowalls(
    const QSet<QnVideoWallResourcePtr>& videowalls,
    bool saveLayout,
    VideoWallCallbackFunction callback)
{
    for (const QnVideoWallResourcePtr& videowall: videowalls)
        saveVideowall(videowall, saveLayout, callback);
}

void QnWorkbenchVideoWallHandler::filterAllowedMediaResources(QnResourceList& resources) const
{
    const auto isAllowed =
        [](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::cross_system))
                return false;

            if (!(QnResourceAccessFilter::isShareableMedia(resource)
                || resource->hasFlags(Qn::desktop_camera)
                || resource->hasFlags(Qn::local_media)))
            {
                return false;
            }

            if (resource.objectCast<QnVirtualCameraResource>())
            {
                return SystemContext::fromResource(resource)->accessController()->hasPermissions(
                    resource, Qn::ViewLivePermission);
            }

            return true;
        };

    QnVirtualCameraResourceList noAccessResources;
    nx::utils::erase_if(resources,
        [&](const QnResourcePtr& resource)
        {
            if (isAllowed(resource))
                return false;

            if (auto camera = resource.objectCast<QnVirtualCameraResource>();
                camera && !camera->hasFlags(Qn::cross_system))
            {
                noAccessResources << camera;
            }

            return true;
        });

    if (!noAccessResources.empty())
    {
        auto showMessage = [this, noAccessResources]()
        {
            const auto count = noAccessResources.size();
            QString text = QnDeviceDependentStrings::getNameFromSet(
                resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Failed to open %n devices on the video wall:", "", count),
                    tr("Failed to open %n cameras on the video wall:", "", count)),
                noAccessResources);
            QString infoText = QnDeviceDependentStrings::getNameFromSet(
                resourcePool(),
                QnCameraDeviceStringSet(
                    tr("You cannot add to the video wall devices for which you do not have View "
                       "Live permission.", "", count),
                    tr("You cannot add to the video wall cameras for which you do not have View "
                       "Live permission.", "", count)),
                noAccessResources);
            QnSessionAwareMessageBox messageBox(mainWindowWidget());
            messageBox.setIcon(QnMessageBoxIcon::Critical);
            messageBox.setText(text);
            messageBox.addCustomWidget(new QnResourceListView(noAccessResources, &messageBox));
            QnWordWrappedLabel infoLabel(&messageBox);
            infoLabel.setText(infoText);
            messageBox.addCustomWidget(&infoLabel);
            messageBox.setStandardButtons(QDialogButtonBox::Ok);
            messageBox.exec();
        };
        executeDelayed(showMessage);
    }
}

void QnWorkbenchVideoWallHandler::filterAllowedLayoutItems(const LayoutResourcePtr& layout) const
{
    const auto itemResources = layoutResources(layout);
    QnResourceList allowedResources{itemResources.cbegin(), itemResources.cend()};
    filterAllowedMediaResources(allowedResources);
    QSet<nx::Uuid> allowedResourceIds;
    for (const auto& allowedResource: allowedResources)
        allowedResourceIds.insert(allowedResource->getId());

    const auto itemsCopy = layout->getItems();
    for (const auto& item: itemsCopy)
    {
        if (!allowedResourceIds.contains(item.resource.id))
            layout->removeItem(item.uuid);
    }
}

void QnWorkbenchVideoWallHandler::setItemOnline(const nx::Uuid &instanceGuid, bool online)
{
    QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(instanceGuid);
    if (index.isNull())
        return;

    QnVideoWallItem item = index.item();
    item.runtimeStatus.online = online;
    index.videowall()->items()->updateItem(item);

    if (item.uuid == workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>())
        updateMode();
}

void QnWorkbenchVideoWallHandler::setItemControlledBy(
    const nx::Uuid& layoutId,
    const nx::Uuid& controllerId,
    bool on)
{
    // The videowall should not be updated while disconnecting.
    if (!systemContext()->connection())
        return;

    const auto currentLayoutId = workbench()->currentLayoutResource()->getId();
    bool needUpdate = false;

    for (const auto& videoWall: resourcePool()->getResources<QnVideoWallResource>())
    {
        // Make a copy before updating items.
        const auto items = videoWall->items()->getItems();
        for (const QnVideoWallItem& item: items)
        {
            if (!on && item.runtimeStatus.controlledBy == controllerId)
            {
                /* Check layouts that were previously controlled by this client. */
                needUpdate |= (item.layout == currentLayoutId && !item.layout.isNull());

                QnVideoWallItem updated(item);
                updated.runtimeStatus.controlledBy = nx::Uuid();
                videoWall->items()->updateItem(updated);
            }
            else if (on && item.layout == layoutId)
            {
                QnVideoWallItem updated(item);
                updated.runtimeStatus.controlledBy = controllerId;
                videoWall->items()->updateItem(updated);
            }
        }
    }

    /* Ignore local changes. */
    if (controllerId != peerId() && needUpdate)
        updateMode();
}

void QnWorkbenchVideoWallHandler::updateMainWindowGeometry(const QnScreenSnaps& screenSnaps)
{
    if (!NX_ASSERT(screenSnaps.isValid()))
        return;

    if (!m_geometrySetter)
        m_geometrySetter.reset(new MultiscreenWidgetGeometrySetter(mainWindowWidget()));

    m_geometrySetter->changeGeometry(screenSnaps);
}

void QnWorkbenchVideoWallHandler::updateControlLayout(
    const QnVideoWallItem& item,
    ItemAction action)
{
    if (action == ItemAction::Changed)
    {
        // index to place updated layout
        int layoutIndex = -1;
        bool wasCurrent = false;

        // check if layout was changed or detached
        for (int i = 0; i < (int) workbench()->layouts().size(); ++i)
        {
            auto layout = workbench()->layout(i);

            if (layout->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>() != item.uuid)
                continue;

            layout->notifyTitleChanged();   //in case of 'online' flag changed

            if (layout->resource() && item.layout == layout->resource()->getId())
                return; //everything is correct, no changes required

            wasCurrent = workbench()->currentLayout() == layout;
            layoutIndex = i;
            layout->setData(Qn::VideoWallItemGuidRole, QVariant::fromValue(nx::Uuid()));
            workbench()->removeLayout(layoutIndex);
            break;
        }

        if (item.layout.isNull() || layoutIndex < 0)
            return;

        // add new layout if needed
        {
            auto layoutResource = resourcePool()->getResourceById<LayoutResource>(item.layout);
            if (!layoutResource)
                return;

            auto layout = workbench()->layout(layoutResource);
            if (layout)
                workbench()->moveLayout(layout, layoutIndex);
            else
                layout = workbench()->insertLayout(layoutResource, layoutIndex);

            layout->setData(Qn::VideoWallItemGuidRole, QVariant::fromValue(item.uuid));
            layout->notifyTitleChanged();
            if (wasCurrent)
                workbench()->setCurrentLayoutIndex(layoutIndex);
        }
    }
    else if (action == ItemAction::Removed)
    {
        QnWorkbenchLayoutList layoutsToClose;
        for (const auto& layout: workbench()->layouts())
        {
            if (layout->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>() == item.uuid)
                layoutsToClose << layout.get();
        }
        menu()->trigger(menu::CloseLayoutAction, layoutsToClose);
    }
}

void QnWorkbenchVideoWallHandler::updateReviewLayout(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallItem& item,
    ItemAction action)
{
    auto layout = QnWorkbenchLayout::instance(videowall);
    if (!layout)
        return;

    auto layoutResource = layout->resource();
    NX_ASSERT(layoutResource);

    auto findCurrentWorkbenchItem =
        [layout, layoutResource, &item]() -> QnWorkbenchItem*
        {
            for (auto workbenchItem: layout->items())
            {
                auto indices = getIndices(layoutResource, workbenchItem->data());
                if (indices.isEmpty())
                    continue;

                // checking existing widgets containing target item
                for (const QnVideoWallItemIndex& index: indices)
                {
                    if (index.uuid() != item.uuid)
                        continue;
                    return workbenchItem;
                }
            }
            return nullptr;
        };

    auto findTargetWorkbenchItem =
        [layout, layoutResource, &item, &videowall]() -> QnWorkbenchItem*
        {
            QnWorkbenchItem* currentItem = nullptr;
            for (auto workbenchItem: layout->items())
            {
                LayoutItemData data = workbenchItem->data();
                auto indices = getIndices(layoutResource, data);
                if (indices.isEmpty())
                    continue;

                // checking existing widgets with same screen sets
                // take any other item on this widget
                const auto other = std::find_if(indices.cbegin(),
                    indices.cend(),
                    [&item](const QnVideoWallItemIndex& idx) { return idx.uuid() != item.uuid; });

                if (other != indices.cend()
                    && (other->item().pcUuid == item.pcUuid)
                    && (screensCoveredByItem(other->item(), videowall)
                        == screensCoveredByItem(item, videowall)))
                {
                    return workbenchItem;
                }

                // our item is the only item on the widget, we can modify it as we want
                if (indices.size() == 1 && indices.first().item().uuid == item.uuid)
                    currentItem = workbenchItem;

                // continue search to more sufficient widgets
            }
            return currentItem;
        };

    auto removeFromWorkbenchItem =
        [layout, layoutResource, &videowall, &item](QnWorkbenchItem* workbenchItem)
        {
            if (!workbenchItem)
                return;

            auto data = workbenchItem->data();
            auto indices = getIndices(layoutResource, data);
            indices.removeAll(QnVideoWallItemIndex(videowall, item.uuid));
            if (indices.isEmpty())
                layout->removeItem(workbenchItem);
            else
                setIndices(layoutResource, data, indices);
        };

    auto addToWorkbenchItem =
        [layoutResource, &videowall, &item](QnWorkbenchItem* workbenchItem)
        {
            if (workbenchItem)
            {
                auto data = workbenchItem->data();
                auto indices = getIndices(layoutResource, data);
                indices << QnVideoWallItemIndex(videowall, item.uuid);
                setIndices(layoutResource, data, indices);
            }
            else
            {
                addItemToLayout(
                    layoutResource,
                    QnVideoWallItemIndexList() << QnVideoWallItemIndex(videowall, item.uuid));
            }
        };

    if (action == ItemAction::Added)
    {
        QnWorkbenchItem* workbenchItem = findTargetWorkbenchItem();
        addToWorkbenchItem(workbenchItem);
    }
    else if (action == ItemAction::Removed)
    {
        removeFromWorkbenchItem(findCurrentWorkbenchItem());
    }
    else if (action == ItemAction::Changed)
    {
        QnWorkbenchItem* workbenchItem = findCurrentWorkbenchItem();
        /* Find new widget for the item. */
        QnWorkbenchItem* newWorkbenchItem = findTargetWorkbenchItem();
        if (workbenchItem != newWorkbenchItem || !newWorkbenchItem)
        { // additional check in case both of them are null
            /* Remove item from the old widget. */
            removeFromWorkbenchItem(workbenchItem);
            addToWorkbenchItem(newWorkbenchItem);
        }
        /*  else item left on the same widget, just do nothing, widget will update itself */

    }

}

bool QnWorkbenchVideoWallHandler::checkLocalFiles(
    const QnVideoWallItemIndex& index,
    const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return true;

    return messages::Videowall::checkLocalFiles(mainWindowWidget(), index,
        layoutResources(layout).values());
}

void QnWorkbenchVideoWallHandler::showLicensesErrorDialog(const QString& detail) const
{
    QnMessageBox messageBox(
        QnMessageBoxIcon::Warning,
        tr("More Video Wall licenses required"),
        detail,
        QDialogButtonBox::Ok,
        QDialogButtonBox::Ok,
        mainWindowWidget());
    messageBox.setEscapeButton(QDialogButtonBox::Ok);

    if (menu()->canTrigger(menu::PreferencesLicensesTabAction))
    {
        auto activateLicensesButton =
            messageBox.addButton(tr("Activate License..."), QDialogButtonBox::HelpRole);
        connect(activateLicensesButton, &QPushButton::clicked, this,
            [this, &messageBox]
            {
                messageBox.accept();
                menu()->trigger(menu::PreferencesLicensesTabAction);
            });
    }

    messageBox.exec();
}

nx::Uuid QnWorkbenchVideoWallHandler::getLayoutController(const nx::Uuid& layoutId)
{
    if (layoutId.isNull())
        return nx::Uuid();

    for (const QnPeerRuntimeInfo& info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.videoWallControlSession != layoutId)
            continue;

        return info.uuid;
    }

    return nx::Uuid();
}

bool QnWorkbenchVideoWallHandler::confirmRemoveResourcesFromLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourceList& resources) const
{
    const QnWorkbenchLayoutsChangeValidator validator(context());
    return validator.confirmChangeVideoWallLayout(layout, resources);
}

void QnWorkbenchVideoWallHandler::saveVideowallAndReviewLayout(
    const QnVideoWallResourcePtr& videowall,
    LayoutResourcePtr reviewLayout,
    VideoWallCallbackFunction callback)
{
    if (!ResourceAccessManager::hasPermissions(videowall, Qn::ReadWriteSavePermission))
        return;

    if (!reviewLayout)
    {
        if (const auto workbenchLayout = QnWorkbenchLayout::instance(videowall))
            reviewLayout = workbenchLayout->resource();
    }

    if (reviewLayout)
    {
        const auto internalCallback =
            [callback, videowall](bool success, const LayoutResourcePtr&)
            {
                if (callback)
                    callback(success, videowall);
            };

        if (reviewLayout->saveAsync(internalCallback))
            return; //< Videowall is already being saved.
    }

    qnResourcesChangesManager->saveVideoWall(videowall, callback);
}

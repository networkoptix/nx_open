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
#include <client_core/client_core_module.h>
#include <common/common_module.h>
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
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_process_runner.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/ui/messages/videowall_messages.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/videowall/utils.h>
#include <nx/vms/client/desktop/videowall/workbench_videowall_shortcut_helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
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
#include <utils/color_space/image_correction.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/uuid_pool.h>
#include <utils/screen_utils.h>
#include <utils/unity_launcher_workaround.h>

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

    QList<int> screens = screensCoveredByItem(firstIdx.item(), firstIdx.videowall()).values();
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
    itemData.uuid = QnUuid::createUuid();
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
    QnUuid pcUuid;
    QSet<int> screens;

    ScreenWidgetKey(const QnUuid &pcUuid, const QSet<int> screens):
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

const QnUuid uuidPoolBase("621992b6-5b8a-4197-af04-1657baab71f0");

class QnVideowallReviewLayoutResource: public LayoutResource
{
public:
    QnVideowallReviewLayoutResource(const QnVideoWallResourcePtr& videowall):
        LayoutResource()
    {
        addFlags(Qn::local);
        setName(videowall->getName());
        setPredefinedCellSpacing(Qn::CellSpacing::Medium);
        setCellAspectRatio(defaultReviewAR);
        setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission));
        setData(Qn::VideoWallResourceRole, QVariant::fromValue(videowall));

        connect(videowall.data(), &QnResource::nameChanged, this, [this](const QnResourcePtr &resource) { setName(resource->getName()); });
    }
};

auto offlineItemOnThisPc = []
    {
        QnUuid pcUuid = appContext()->localSettings()->pcUuid();
        NX_ASSERT(!pcUuid.isNull(), "Invalid pc state.");
        return [pcUuid](const QnVideoWallItem& item)
            {
                return !pcUuid.isNull() && item.pcUuid == pcUuid && !item.runtimeStatus.online;
            };
    };

QSet<QnUuid> layoutResourceIds(const QnLayoutResourcePtr& layout)
{
    QSet<QnUuid> result;
    for (const auto& item: layout->getItems())
        result << item.resource.id;
    return result;
}

LayoutSnapshotManager* snapshotManager()
{
    return appContext()->currentSystemContext()->layoutSnapshotManager();
}

// Main window geometry should have position in physical coordinates and size - in logical.
QRect mainWindowGeometry(const QnScreenSnaps& snaps)
{
    if (!snaps.isValid())
        return {};

    return snaps.geometry(nx::gui::Screens::logicalGeometries());
}

} // namespace

//--------------------------------------------------------------------------------------------------

// Workaround for the geometry changes. In MacOsX setGeometry function call on main window
// hangs all drawing to the whole client. As workaround we turn off updates before geometry changes
// and return them back when resize operations are complete.
class QnWorkbenchVideoWallHandler::GeometrySetter: public QObject
{
    using base_type = QObject;

public:
    GeometrySetter(QWidget* target, QObject* parent = nullptr);

    void changeGeometry(const QRect& geometry);

    bool eventFilter(QObject* watched, QEvent* event);

private:
    QWidget* const m_target;
    QSize m_finalSize;
};

QnWorkbenchVideoWallHandler::GeometrySetter::GeometrySetter(QWidget* target, QObject* parent):
    base_type(parent),
    m_target(target)
{
    if (nx::build_info::isMacOsX())
        m_target->installEventFilter(this);
}

void QnWorkbenchVideoWallHandler::GeometrySetter::changeGeometry(const QRect& geometry)
{
    const auto targetSize = geometry.size();
    if (nx::build_info::isMacOsX() && m_target->geometry().size() != targetSize)
    {
        m_target->setUpdatesEnabled(false);
        m_finalSize = targetSize;
    }
    m_target->setGeometry(geometry);
}

bool QnWorkbenchVideoWallHandler::GeometrySetter::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_finalSize.isEmpty() && watched == m_target && event->type() == QEvent::Resize)
    {
        const auto currentSize = static_cast<QResizeEvent*>(event)->size();
        if (m_finalSize == currentSize)
        {
            m_finalSize = QSize();
            const auto setUpdatesEnabled = [this]() { m_target->setUpdatesEnabled(true); };
            static constexpr int kSomeSmallDelayMs = 1000;
            executeDelayedParented(setUpdatesEnabled, kSomeSmallDelayMs, this);
        }
    }
    return base_type::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------

QnWorkbenchVideoWallHandler::QnWorkbenchVideoWallHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_licensesHelper(new nx::vms::license::VideoWallLicenseUsageHelper(systemContext(), this)),
    #ifdef _DEBUG
        /* Limit by reasonable size. */
        m_uuidPool(new QnUuidPool(uuidPoolBase, 256))
    #else
        m_uuidPool(new QnUuidPool(uuidPoolBase, 16384))
    #endif
{
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

    QnUuid pcUuid = appContext()->localSettings()->pcUuid();
    if (pcUuid.isNull())
    {
        pcUuid = QnUuid::createUuid();
        appContext()->localSettings()->pcUuid = pcUuid;
    }
    m_controlMode.pcUuid = pcUuid.toString();

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
            if (info.uuid == systemContext()->peerId())
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
            if (info.uuid < systemContext()->peerId())
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

        connect(action(action::DelayedOpenVideoWallItemAction), &QAction::triggered, this,
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

        connect(action(action::NewVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered);
        connect(action(action::RemoveFromServerAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_deleteVideoWallAction_triggered);
        connect(action(action::AttachToVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered);
        connect(action(action::ClearVideoWallScreen), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered);
        connect(action(action::DeleteVideoWallItemAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_deleteVideoWallItemAction_triggered);
        connect(action(action::StartVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_startVideoWallAction_triggered);
        connect(action(action::StopVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_stopVideoWallAction_triggered);
        connect(action(action::RenameVideowallEntityAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_renameAction_triggered);
        connect(action(action::IdentifyVideoWallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_identifyVideoWallAction_triggered);
        connect(action(action::StartVideoWallControlAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered);
        connect(action(action::OpenVideoWallReviewAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_openVideoWallReviewAction_triggered);
        connect(action(action::SaveCurrentVideoWallReviewAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_saveCurrentVideoWallReviewAction_triggered);
        connect(action(action::SaveVideoWallReviewAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered);
        connect(action(action::DropOnVideoWallItemAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_dropOnVideoWallItemAction_triggered);
        connect(action(action::PushMyScreenToVideowallAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_pushMyScreenToVideowallAction_triggered);
        connect(action(action::VideowallSettingsAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_videowallSettingsAction_triggered);
        connect(action(action::SaveVideowallMatrixAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_saveVideowallMatrixAction_triggered);
        connect(action(action::LoadVideowallMatrixAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_loadVideowallMatrixAction_triggered);
        connect(action(action::DeleteVideowallMatrixAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_deleteVideowallMatrixAction_triggered);
        connect(action(action::VideoWallScreenSettingsAction), &QAction::triggered, this,
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
    if (!NX_ASSERT(layout->systemContext() == appContext()->currentSystemContext()))
        return;

    layout->setCellSpacing(0.0);

    auto reset =
        [this](const QnVideoWallItemIndexList& items, const LayoutResourcePtr& layout)
        {
            updateItemsLayout(items, layout->getId());
        };

    const bool needToSave = !layout->isFile()
        && (layout->hasFlags(Qn::local) || snapshotManager()->isModified(layout));

    if (needToSave)
    {
        auto callback =
            [this, items, reset](bool success, const LayoutResourcePtr& layout)
            {
                if (!success)
                    showFailedToApplyChanges();
                else
                    reset(items, layout);
                updateMode();
            };
        snapshotManager()->save(layout, callback);
        systemContext()->resourcePropertyDictionary()->saveParamsAsync(layout->getId());
    }
    else
    {
        reset(items, layout);
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
    if (firstLayout && (firstLayout->hasFlags(Qn::local) || snapshotManager()->isModified(firstLayout)))
        unsavedLayouts << firstLayout;

    if (secondLayout && (secondLayout->hasFlags(Qn::local) || snapshotManager()->isModified(secondLayout)))
        unsavedLayouts << secondLayout;

    auto swap =
        [this](
            const QnVideoWallItemIndex firstIndex,
            const LayoutResourcePtr& firstLayout,
            const QnVideoWallItemIndex& secondIndex,
            const LayoutResourcePtr& secondLayout)
        {
            QnVideoWallItem firstItem = firstIndex.item();
            firstItem.layout = firstLayout ? firstLayout->getId() : QnUuid();
            firstIndex.videowall()->items()->updateItem(firstItem);

            QnVideoWallItem secondItem = secondIndex.item();
            secondItem.layout = secondLayout ? secondLayout->getId() : QnUuid();
            secondIndex.videowall()->items()->updateItem(secondItem);

            saveVideowalls(QSet<QnVideoWallResourcePtr>()
                << firstIndex.videowall()
                << secondIndex.videowall());
        };

    if (!unsavedLayouts.isEmpty())
    {

        auto callback =
            [this, firstIndex, firstLayout, secondIndex, secondLayout, swap]
            (bool success, const LayoutResourcePtr& /*layout*/)
            {
                if (!success)
                    showFailedToApplyChanges();
                else
                    swap(firstIndex, firstLayout, secondIndex, secondLayout);
            };

        if (unsavedLayouts.size() == 1)
        {
            snapshotManager()->save(unsavedLayouts.first(), callback);
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
                    callback(bothSuccess, LayoutResourcePtr());
                    counter->deleteLater();
                });

            auto localCallback =
                [&bothSuccess, counter](bool success, const LayoutResourcePtr& /*layout*/)
                {
                    bothSuccess &= success;
                    counter->decrement();
                };
            for (const LayoutResourcePtr& layout: unsavedLayouts)
                snapshotManager()->save(layout, localCallback);
        }

    }
    else
    {
        swap(firstIndex, firstLayout, secondIndex, secondLayout);
    }
}

void QnWorkbenchVideoWallHandler::updateItemsLayout(
    const QnVideoWallItemIndexList& items,
    const QnUuid& layoutId)
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
        [this](const QnVideoWallResourceList& successfullySaved)
        {
            cleanupUnusedLayouts(successfullySaved);
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

void QnWorkbenchVideoWallHandler::openNewWindow(QnUuid videoWallId, const QnVideoWallItem& item)
{
    if (!NX_ASSERT(connection(), "Client must be connected while we are running the video wall"))
        return;

    nx::vms::client::core::LogonData logonData;
    logonData.address = connection()->address();
    // Connection credentials will be constructed by callee from videowall IDs.

    const int leftmostScreen = item.screenSnaps.left().screenIndex;

    QStringList arguments;
    arguments
        << "--videowall"
        << videoWallId.toString()
        << "--videowall-instance"
        << item.uuid.toString()
        << "--auth"
        << QnStartupParameters::createAuthenticationString(logonData);

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
        menu()->triggerForced(action::OpenInNewTabAction, layout);
}

void QnWorkbenchVideoWallHandler::closeInstanceDelayed()
{
    menu()->trigger(action::DelayedForcedExitAction);
}

void QnWorkbenchVideoWallHandler::sendMessage(const QnVideoWallControlMessage& message, bool cached)
{
    NX_ASSERT(m_controlMode.active);

    auto connection = messageBusConnection();
    if (!NX_ASSERT(connection))
        return;

    auto videoWallManager = connection->getVideowallManager(Qn::kSystemAccess);

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
    const QnUuid& controllerUuid,
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
            QnUuid guid(message[uuidKey]);
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

            QnUuid uuid = QnUuid(message[uuidKey]);
            if (workbench()->currentLayout()->item(uuid))
                return;

            QnUuid resourceId(message[resourceKey]);
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
            QnUuid uuid = QnUuid(message[uuidKey]);
            if (QnWorkbenchItem* item = workbench()->currentLayout()->item(uuid))
                workbench()->currentLayout()->removeItem(item);
            break;
        }
        // Keep in sync with QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged.
        case QnVideoWallControlMessage::LayoutItemDataChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            QnUuid uuid = QnUuid(message[uuidKey]);
            auto item = workbench()->currentLayout()->item(uuid);
            if (!item)
                return;

            Qn::ItemDataRole role = static_cast<Qn::ItemDataRole>(message[roleKey].toInt());

            switch (role)
            {
                case Qn::ItemGeometryRole:
                {
                    QRect data = QJson::deserialized<QRect>(value);
                    item->setData(role, data);
                    NX_VERBOSE(this, "RECEIVER: Item %1 geometry changed to %2", uuid, data);
                    break;
                }
                case Qn::ItemZoomRectRole:
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
                    menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
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
                    menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
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
                    menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
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
                    QColor data = QJson::deserialized<QColor>(value);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemSliderWindowRole:
                case Qn::ItemSliderSelectionRole:
                {
                    QnTimePeriod data = QJson::deserialized<QnTimePeriod>(value);
                    item->setData(role, data);
                    menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
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
            auto item = workbench()->currentLayout()->item(QnUuid(message[uuidKey]));
            auto zoomTargetItem = workbench()->currentLayout()->item(QnUuid(message[zoomUuidKey]));
            NX_VERBOSE(this, "RECEIVER: ZoomLinkAdded message %1 %2 %3",
                message, item, zoomTargetItem);
            if (!item || !zoomTargetItem)
                return;
            workbench()->currentLayout()->addZoomLink(item, zoomTargetItem);
            break;
        }
        case QnVideoWallControlMessage::ZoomLinkRemoved:
        {
            auto item = workbench()->currentLayout()->item(QnUuid(message[uuidKey]));
            auto zoomTargetItem = workbench()->currentLayout()->item(QnUuid(message[zoomUuidKey]));
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
                menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
            break;
        }
        case QnVideoWallControlMessage::NavigatorPlayingChanged:
        {
            navigator()->setPlaying(QnLexical::deserialized<bool>(message[valueKey]));
            menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
            break;
        }
        case QnVideoWallControlMessage::NavigatorSpeedChanged:
        {
            navigator()->setSpeed(message[speedKey].toDouble());
            if (message.contains(kPositionUsecKey))
                navigator()->setPosition(message[kPositionUsecKey].toLongLong());
            menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
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
                menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
            }
            break;
        }
        case QnVideoWallControlMessage::MotionSelectionChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            auto regions = QJson::deserialized<MotionSelection>(value);
            auto widget = dynamic_cast<QnMediaResourceWidget*>(
                display()->widget(QnUuid(message[uuidKey])));
            if (widget)
                widget->setMotionSelection(regions);
            break;
        }
        case QnVideoWallControlMessage::MediaDewarpingParamsChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            auto params = QJson::deserialized<nx::vms::api::dewarping::MediaData>(value);
            auto widget = dynamic_cast<QnMediaResourceWidget*>(
                display()->widget(QnUuid(message[uuidKey])));
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
            const auto items = QJson::deserialized<std::vector<QnUuid>>(message[itemsKey].toUtf8());

            const auto layout = workbench()->currentLayout()->resource();
            if (!layout)
                return;

            LayoutItemIndexList layoutItems;
            for (const auto& id: items)
                layoutItems.push_back(LayoutItemIndex(layout, id));

            ui::action::Parameters parameters(layoutItems);
            parameters.setArgument(Qn::ResolutionModeRole, mode);
            menu()->trigger(ui::action::RadassAction, parameters);
            break;
        }
        default:
            break;
    }
}

void QnWorkbenchVideoWallHandler::storeMessage(
    const QnVideoWallControlMessage& message,
    const QnUuid& controllerUuid,
    qint64 sequence)
{
    m_videoWallMode.storedMessages[controllerUuid][sequence] = message;
    NX_VERBOSE(this, "RECEIVER: store message %1", message);
}

void QnWorkbenchVideoWallHandler::restoreMessages(const QnUuid& controllerUuid, qint64 sequence)
{
    StoredMessagesHash &stored = m_videoWallMode.storedMessages[controllerUuid];
    while (stored.contains(sequence + 1))
    {
        QnVideoWallControlMessage message = stored.take(++sequence);
        NX_VERBOSE(this, "RECEIVER: restored message %1", message);
        handleMessage(message, controllerUuid, sequence);
    }
}

bool QnWorkbenchVideoWallHandler::canStartControlMode(const QnUuid& layoutId) const
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

void QnWorkbenchVideoWallHandler::showFailedToApplyChanges() const
{
    QnMessageBox::critical(mainWindowWidget(), tr("Failed to apply changes"));
}

void QnWorkbenchVideoWallHandler::showControlledByAnotherUserMessage() const
{
    QnMessageBox::warning(mainWindowWidget(),
        tr("Screen is being controlled by another user"),
        tr("Control session cannot be started."));
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
        connect(action(action::RadassAction), &QAction::triggered, this,
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

        QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
        localInfo.data.videoWallControlSession = layoutResource->getId();
        runtimeInfoManager()->updateLocalItem(localInfo);

        setItemControlledBy(localInfo.data.videoWallControlSession, localInfo.uuid, true);
    }
    else
    {
        action(action::RadassAction)->disconnect(this);

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

        QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
        localInfo.data.videoWallControlSession = QnUuid();
        runtimeInfoManager()->updateLocalItem(localInfo);

        setItemControlledBy(localInfo.data.videoWallControlSession, localInfo.uuid, false);
    }
}

void QnWorkbenchVideoWallHandler::updateMode()
{
    QnWorkbenchLayout* layout = workbench()->currentLayout();

    QnUuid itemUuid = layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
    bool control = false;
    if (!itemUuid.isNull())
    {
        auto index = resourcePool()->getVideoWallItemByUuid(itemUuid);
        auto item = index.item();
        if (item.runtimeStatus.online
            && (item.runtimeStatus.controlledBy.isNull()
                || item.runtimeStatus.controlledBy == systemContext()->peerId())
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

        NX_ERROR(this, message.arg(m_videoWallMode.guid.toString()));

        VideoWallShortcutHelper::setVideoWallAutorunEnabled(m_videoWallMode.guid, false);
        closeInstanceDelayed();
        return;
    }

    QnUuid pcUuid = appContext()->localSettings()->pcUuid();
    if (pcUuid.isNull())
    {
        NX_ERROR(this, "Warning: pc UUID is null, cannot start videowall %1 on this pc",
            m_videoWallMode.guid.toString());

        closeInstanceDelayed();
        return;
    }

    bool master = m_videoWallMode.instanceGuid.isNull();
    if (master)
    {
        foreach(const QnVideoWallItem &item, videoWall->items()->getItems())
        {
            if (item.pcUuid != pcUuid || item.runtimeStatus.online)
                continue;

            openNewWindow(m_videoWallMode.guid, item);
        }
        closeInstanceDelayed();
    }
    else
    {
        openVideoWallItem(videoWall);
    }
}

QnVideoWallItemIndexList QnWorkbenchVideoWallHandler::targetList() const
{
    auto layout = workbench()->currentLayout()->resource();
    if (!layout)
        return QnVideoWallItemIndexList();

    QnUuid currentId = layout->getId();

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

    LayoutResourcePtr layout = sourceLayout ?
        sourceLayout->clone() :
        LayoutResourcePtr(new LayoutResource());

    layout->setIdUnsafe(m_uuidPool->getFreeId());
    layout->addFlags(Qn::local);

    if (!sourceLayout)
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
        QSet<QnUuid> used;
        for (const QnVideoWallItem& item: videowall->items()->getItems())
            used << item.layout;

        for (const QnVideoWallMatrix& matrix: videowall->matrices()->getItems())
        {
            for (const auto& layoutId: matrix.layoutByItem.values())
                used << layoutId;
        }

        QnUuid videoWallId = videowall->getId();
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
        if (menu()->triggerIfPossible(action::RemoveFromServerAction, layout))
            resourcePool()->removeResource(layout);
    }
}

/*------------------------------------ HANDLERS ------------------------------------------*/

void QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered()
{
    if (nx::vms::common::saas::saasInitialized(context()->systemContext()))
    {
        if (context()->systemContext()->saasServiceManager()->saasShutDown())
        {
            const auto saasState = context()->systemContext()->saasServiceManager()->saasState();

            const auto caption = tr("System shut down");
            const auto text = tr("To add a Video Wall, the System should be in active state. %1")
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
    videoWall->setIdUnsafe(QnUuid::createUuid());
    videoWall->setName(proposedName);
    videoWall->setAutorun(true);

    // No need to backup newly created videowall.
    auto applyChangesFunction = ResourcesChangesManager::VideoWallChangesFunction();
    auto callbackFunction =
        [this](bool success, const QnVideoWallResourcePtr& videoWall)
        {
            // Cannot capture the resource directly because real resource pointer may differ if the
            // transaction is received before the request callback.
            NX_ASSERT(videoWall);
            if (success && videoWall)
            {
                menu()->trigger(action::SelectNewItemAction, videoWall);
                menu()->trigger(action::OpenVideoWallReviewAction, videoWall);
            }
        };

    qnResourcesChangesManager->saveVideoWall(videoWall, applyChangesFunction, callbackFunction);
}

void QnWorkbenchVideoWallHandler::at_deleteVideoWallAction_triggered()
{
    QnVideoWallResourceList videowalls = menu()->currentParameters(sender()).resources()
        .filtered<QnVideoWallResource>(
            [this](const QnResourcePtr& resource)
            {
                return menu()->canTrigger(action::RemoveFromServerAction, resource);
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

    QScopedPointer<QnAttachToVideowallDialog> dialog(
        new QnAttachToVideowallDialog(mainWindowWidget()));
    dialog->loadFromResource(videoWall);
    if (!dialog->exec())
        return;

    qnResourcesChangesManager->saveVideoWall(videoWall,
        [d = dialog.data()](const QnVideoWallResourcePtr& videoWall)
        {
            d->submitToResource(videoWall);
        });

    menu()->trigger(action::OpenVideoWallReviewAction, videoWall);
}

void QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered()
{
    QnVideoWallItemIndexList items = menu()->currentParameters(sender()).videoWallItems();

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
            QSet<QnUuid> resourceIds = layoutResourceIds(layout);
            auto removedResources = layout->resourcePool()->getResourcesByIds(resourceIds);
            if (!confirmRemoveResourcesFromLayout(layout, removedResources))
                break;
        }

        existingItem.layout = QnUuid();
        index.videowall()->items()->updateItem(existingItem);
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls, /*saveLayout*/ false,
        [this](const QnVideoWallResourceList& successfullySaved)
        {
            cleanupUnusedLayouts(successfullySaved);
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
        [this](const QnVideoWallResourceList& successfullySaved)
        {
            cleanupUnusedLayouts(successfullySaved);
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

    auto videoWallManager = connection->getVideowallManager(Qn::kSystemAccess);

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
        menu()->currentParameters(sender()).argument<QnUuid>(Qn::VideoWallGuidRole);
    m_videoWallMode.instanceGuid =
        menu()->currentParameters(sender()).argument<QnUuid>(Qn::VideoWallItemGuidRole);
    m_videoWallMode.opening = true;
    submitDelayedItemOpen();
}

void QnWorkbenchVideoWallHandler::at_renameAction_triggered()
{
    using NodeType = ResourceTree::NodeType;

    const auto parameters = menu()->currentParameters(sender());

    const auto nodeType = parameters.argument<NodeType>(Qn::NodeTypeRole, NodeType::resource);
    QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();

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

    auto videoWallManager = connection->getVideowallManager(Qn::kSystemAccess);

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
    LayoutResourcePtr layout(new QnVideowallReviewLayoutResource(videoWall));
    layout->setIdUnsafe(QnUuid::createUuid());
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

    menu()->trigger(action::OpenInNewTabAction, layout);

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
    if (!NX_ASSERT(layout->isVideoWallReviewLayout()))
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
    QnUuid targetUuid = parameters.argument(Qn::VideoWallItemGuidRole).value<QnUuid>();
    QnVideoWallItemIndex targetIndex = resourcePool()->getVideoWallItemByUuid(targetUuid);
    if (!targetIndex.isValid())
        return;

    if (!ResourceAccessManager::hasPermissions(targetIndex.videowall(), Qn::ReadWriteSavePermission))
        return;

    /* Layout that is currently on the drop-target item. */
    auto currentLayout = resourcePool()->getResourceById<LayoutResource>(targetIndex.item().layout);

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

    checkResourcesPermissions(targetResources);

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

    QnUuid videoWallId = targetIndex.videowall()->getId();

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
    const auto user = context()->user();
    if (!user)
        return;

    const auto desktopCameraId = core::DesktopResource::calculateUniqueId(
        systemContext()->peerId(), user->getId());

    const auto desktopCamera = resourcePool()->getResourceByPhysicalId<QnVirtualCameraResource>(
        desktopCameraId);
    if (!desktopCamera || !desktopCamera->hasFlags(Qn::desktop_camera))
        return;

    const auto parameters = menu()->currentParameters(sender());
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();

    for (const auto& index: videoWallItems)
    {
        menu()->trigger(action::DropOnVideoWallItemAction, action::Parameters(desktopCamera)
            .withArgument(Qn::VideoWallItemGuidRole, index.uuid()));
    }
}

void QnWorkbenchVideoWallHandler::at_videowallSettingsAction_triggered()
{
    auto videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    QScopedPointer<QnVideowallSettingsDialog> dialog(new QnVideowallSettingsDialog(mainWindowWidget()));
    dialog->loadFromResource(videowall);

    if (!dialog->exec())
        return;

    dialog->submitToResource(videowall);
    saveVideowall(videowall);
}

void QnWorkbenchVideoWallHandler::at_saveVideowallMatrixAction_triggered()
{
    auto videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    QnVideoWallMatrix matrix;
    matrix.name = tr("New Matrix %1").arg(videowall->matrices()->getItems().size() + 1);
    matrix.uuid = QnUuid::createUuid();

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

        QnUuid layoutUuid = matrix.layoutByItem[item.uuid];
        if (!layoutUuid.isNull() && !resourcePool()->getResourceById<QnLayoutResource>(layoutUuid))
            layoutUuid = QnUuid();

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

    menu()->trigger(action::LayoutSettingsAction, layout);
}

void QnWorkbenchVideoWallHandler::at_radassAction_triggered()
{
    if (!m_controlMode.active)
        return;

    const auto parameters = menu()->currentParameters(sender());

    const auto mode = parameters.argument(Qn::ResolutionModeRole).toInt();

    std::vector<QnUuid> items;
    for (const auto& item: parameters.layoutItems())
        items.push_back(item.uuid());

    QnVideoWallControlMessage message(QnVideoWallControlMessage::RadassModeChanged);
    message[valueKey] = QString::number(mode);
    message[itemsKey] = QString::fromUtf8(QJson::serialized(items));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_resPool_resourceAdded(const QnResourcePtr &resource)
{
    /* Exclude from pool all existing resources ids. */
    m_uuidPool->markAsUsed(resource->getId());

    auto videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (!videoWall)
        return;

    auto getServerUrl =
        [this]
        {
            nx::network::SocketAddress connectionAddress;
            if (auto connection = this->connection(); NX_ASSERT(connection))
                connectionAddress = connection->address();

            return nx::network::url::Builder()
                .setScheme(nx::network::http::kSecureUrlSchemeName)
                .setEndpoint(connectionAddress)
                .toUrl();
        };

    auto handleAutoRunChanged =
        [getServerUrl](const QnVideoWallResourcePtr& videoWall)
        {
            if (videoWall && videoWall->pcs()->hasItem(appContext()->localSettings()->pcUuid()))
            {
                VideoWallShortcutHelper::setVideoWallAutorunEnabled(
                    videoWall->getId(),
                    videoWall->isAutorun(),
                    getServerUrl());
            }
        };
    handleAutoRunChanged(videoWall);

    connect(videoWall.get(), &QnVideoWallResource::autorunChanged, this, handleAutoRunChanged);

    connect(videoWall.get(), &QnVideoWallResource::pcAdded, this,
        [getServerUrl](const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
        {
            if (pc.uuid != appContext()->localSettings()->pcUuid())
                return;

            VideoWallShortcutHelper::setVideoWallAutorunEnabled(
                videoWall->getId(),
                videoWall->isAutorun(),
                getServerUrl());
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
                getServerUrl());

            auto handleTimelineEnabledChanged =
                [this](const QnVideoWallResourcePtr& videoWall)
                {
                    qnRuntime->setVideoWallWithTimeLine(videoWall->isTimelineEnabled());
                    menu()->triggerIfPossible(action::ShowTimeLineOnVideowallAction);
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
    QnUuid controller = getLayoutController(item.layout);
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

    QnUuid controllerUuid = QnUuid(message[pcUuidKey]);
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
    message[uuidKey] = widget->item()->uuid().toString();
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
    message[uuidKey] = widget->item()->uuid().toString();
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
    message[uuidKey] = workbench()->item(role) ? workbench()->item(role)->uuid().toString() : QString();
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *item)
{
    connect(item, &QnWorkbenchItem::dataChanged, this,
        &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);

    if (!m_controlMode.active)
        return;

    const auto itemUuid = item->uuid().toString();

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
    message[uuidKey] = item->uuid().toString();
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded(
    QnWorkbenchItem* item,
    QnWorkbenchItem* zoomTargetItem)
{
    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::ZoomLinkAdded);
    message[uuidKey] = item->uuid().toString();
    message[zoomUuidKey] = zoomTargetItem->uuid().toString();
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
    message[uuidKey] = item->uuid().toString();
    message[zoomUuidKey] = zoomTargetItem->uuid().toString();
    sendMessage(message);
    NX_VERBOSE(this, "SENDER: zoom Link removed %1 %2", item->uuid(), zoomTargetItem->uuid());
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_dataChanged(Qn::ItemDataRole role)
{
    if (role == Qn::VideoWallItemGuidRole || role == Qn::VideoWallResourceRole)
    {
        updateMode();
        return;
    }
}

// Keep in sync with QnWorkbenchVideoWallHandler::handleMessage.
void QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged(Qn::ItemDataRole role)
{
    if (!m_controlMode.active)
        return;

    static const QSet<Qn::ItemDataRole> kIgnoreOnLayoutChange =
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

        case Qn::ItemZoomRectRole:
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
            QJson::serialize(value, &json);
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
    message[uuidKey] = item->uuid().toString();
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
        qnResourcesChangesManager->saveVideoWall(videowall, {}, callback);
}

void QnWorkbenchVideoWallHandler::saveVideowalls(
    const QSet<QnVideoWallResourcePtr>& videowalls,
    bool saveLayout,
    std::function<void(const QnVideoWallResourceList& successfullySaved)> callback)
{
    for (const QnVideoWallResourcePtr& videowall: videowalls)
        saveVideowall(videowall, saveLayout);
}

bool QnWorkbenchVideoWallHandler::saveReviewLayout(
    const LayoutResourcePtr& layoutResource,
    std::function<void(int, ec2::ErrorCode)> callback)
{
    return saveReviewLayout(workbench()->layout(layoutResource), callback);
}

bool QnWorkbenchVideoWallHandler::saveReviewLayout(
    QnWorkbenchLayout* layout,
    std::function<void(int, ec2::ErrorCode)> callback)
{
    if (!layout)
        return false;

    auto connection = messageBusConnection();
    if (!NX_ASSERT(connection))
        return false;

    auto videoWallManager = connection->getVideowallManager(Qn::kSystemAccess);

    QSet<QnVideoWallResourcePtr> videowalls;
    for (QnWorkbenchItem* workbenchItem : layout->items())
    {
        LayoutItemData data = workbenchItem->data();
        auto indices = getIndices(layout->resource(), data);
        if (indices.isEmpty())
            continue;
        QnVideoWallItemIndex firstIdx = indices.first();
        QnVideoWallResourcePtr videowall = firstIdx.videowall();
        QnVideoWallItem item = firstIdx.item();
        QSet<int> screenIndices = screensCoveredByItem(item, videowall);
        if (!videowall->pcs()->hasItem(item.pcUuid))
            continue;
        QnVideoWallPcData pc = videowall->pcs()->getItem(item.pcUuid);
        if (screenIndices.size() < 1)
            continue;
        foreach(int screenIndex, screenIndices)
            pc.screens[screenIndex].layoutGeometry = data.combinedGeometry.toRect();
        videowall->pcs()->updateItem(pc);
        videowalls << videowall;
    }

    // TODO: #sivanov Refactor saving to simplier logic. Sometimes saving is not required.
    for (const QnVideoWallResourcePtr& videowall: videowalls)
    {
        nx::vms::api::VideowallData apiVideowall;
        ec2::fromResourceToApi(videowall, apiVideowall);
        videoWallManager->save(apiVideowall, callback, this);
    }

    return !videowalls.isEmpty();
}

void QnWorkbenchVideoWallHandler::checkResourcesPermissions(QnResourceList& resources)
{
    QnVirtualCameraResourceList noAccessResources;
    nx::utils::erase_if(resources,
        [&noAccessResources](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::cross_system))
                return true;

            const auto accessController =
                SystemContext::fromResource(resource)->accessController();
            if (accessController->hasPermissions(resource, Qn::ViewLivePermission))
                return false;
            if (auto virtualCamera = resource.dynamicCast<QnVirtualCameraResource>())
                noAccessResources << virtualCamera;
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

void QnWorkbenchVideoWallHandler::setItemOnline(const QnUuid &instanceGuid, bool online)
{
    QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(instanceGuid);
    if (index.isNull())
        return;

    QnVideoWallItem item = index.item();
    item.runtimeStatus.online = online;
    index.videowall()->items()->updateItem(item);

    if (item.uuid == workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).value<QnUuid>())
        updateMode();
}

void QnWorkbenchVideoWallHandler::setItemControlledBy(
    const QnUuid& layoutId,
    const QnUuid& controllerId,
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
                updated.runtimeStatus.controlledBy = QnUuid();
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
    if (controllerId != systemContext()->peerId() && needUpdate)
        updateMode();
}

void QnWorkbenchVideoWallHandler::updateMainWindowGeometry(const QnScreenSnaps& screenSnaps)
{
    if (!NX_ASSERT(screenSnaps.isValid()))
        return;

    // Target geometry will be used in QWidget::setGeometry(), so it expects physical coordinates
    // for window position and logical coordinates for it's size.
    const QRect targetGeometry = mainWindowGeometry(screenSnaps);

    if (!NX_ASSERT(targetGeometry.isValid()))
        return;

    if (!m_geometrySetter)
        m_geometrySetter.reset(new GeometrySetter(mainWindowWidget()));

    // Geometry must be changed twice: after the first time window screen is updated correctly, so
    // on the second time it is correctly used for QWidget internal dpi-aware calculations.
    m_geometrySetter->changeGeometry(targetGeometry);
    executeDelayedParented(
        [this, targetGeometry]{ m_geometrySetter->changeGeometry(targetGeometry); },
        this);
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
        for (int i = 0; i < workbench()->layouts().size(); ++i)
        {
            auto layout = workbench()->layout(i);

            if (layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>() != item.uuid)
                continue;

            layout->notifyTitleChanged();   //in case of 'online' flag changed

            if (layout->resource() && item.layout == layout->resource()->getId())
                return; //everything is correct, no changes required

            wasCurrent = workbench()->currentLayout() == layout;
            layoutIndex = i;
            layout->setData(Qn::VideoWallItemGuidRole, QVariant::fromValue(QnUuid()));
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
            if (layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>() == item.uuid)
                layoutsToClose << layout.get();
        }
        menu()->trigger(ui::action::CloseLayoutAction, layoutsToClose);
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
                addItemToLayout(layoutResource,
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

    if (menu()->canTrigger(ui::action::PreferencesLicensesTabAction))
    {
        auto activateLicensesButton =
            messageBox.addButton(tr("Activate License..."), QDialogButtonBox::HelpRole);
        connect(activateLicensesButton, &QPushButton::clicked, this,
            [this, &messageBox]
            {
                messageBox.accept();
                menu()->trigger(ui::action::PreferencesLicensesTabAction);
            });
    }

    messageBox.exec();
}

QnUuid QnWorkbenchVideoWallHandler::getLayoutController(const QnUuid& layoutId)
{
    if (layoutId.isNull())
        return QnUuid();

    for (const QnPeerRuntimeInfo& info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.videoWallControlSession != layoutId)
            continue;

        return info.uuid;
    }

    return QnUuid();
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
        const auto saveLayoutCallback =
            [id = reviewLayout->getId()](int /*reqId*/, ec2::ErrorCode errorCode)
            {
                snapshotManager()->markBeingSaved(id, false);
                if (errorCode != ec2::ErrorCode::ok)
                    return;

                snapshotManager()->markChanged(id, false);
            };

        if (saveReviewLayout(reviewLayout, saveLayoutCallback))
            return;
    }

    qnResourcesChangesManager->saveVideoWall(videowall, {}, callback);
}

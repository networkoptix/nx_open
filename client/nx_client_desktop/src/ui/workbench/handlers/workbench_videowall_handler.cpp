#include "workbench_videowall_handler.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <api/runtime_info_manager.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/algorithm/cxx11/copy_if.hpp>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_message_processor.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_installations_manager.h>
#include <client/client_startup_parameters.h>

#include <core/resource_access/resource_access_filter.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/resource_runtime_data.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource/videowall_matrix_index.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <plugins/resource/desktop_camera/desktop_resource_base.h>

#include <recording/time_period.h>

#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_videowall_manager.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/radass/radass_types.h>
#include <ui/dialogs/layout_name_dialog.h> // TODO: #GDM #VW refactor
#include <ui/dialogs/attach_to_videowall_dialog.h>
#include <ui/dialogs/resource_properties/videowall_settings_dialog.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/widgets/views/resource_list_view.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/extensions/workbench_layout_change_validator.h>

#include <utils/color_space/image_correction.h>
#include <utils/common/checked_cast.h>

#include <nx/client/desktop/ui/messages/videowall_messages.h>

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include <utils/license_usage_helper.h>
#include <utils/common/uuid_pool.h>
#include <nx/utils/counter.h>
#include <utils/unity_launcher_workaround.h>
#include <utils/common/delayed.h>

#include <nx/vms/utils/platform/autorun.h>
#include <nx/client/desktop/ui/workbench/layouts/layout_factory.h>


//#define SENDER_DEBUG
//#define RECEIVER_DEBUG

using nx::client::desktop::utils::UnityLauncherWorkaround;
using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace {
#define PARAM_KEY(KEY) const QLatin1String KEY##Key(BOOST_PP_STRINGIZE(KEY));
PARAM_KEY(sequence)
PARAM_KEY(pcUuid)
PARAM_KEY(uuid)
PARAM_KEY(zoomUuid)
PARAM_KEY(resource)
PARAM_KEY(value)
PARAM_KEY(role)
PARAM_KEY(position)
PARAM_KEY(rotation)
PARAM_KEY(speed)
PARAM_KEY(geometry)
PARAM_KEY(zoomRect)
PARAM_KEY(checkedButtons)
PARAM_KEY(items)

#if defined(SENDER_DEBUG) || defined(RECEIVER_DEBUG)
static QByteArray debugRole(int role)
{
    switch (role)
    {
        case Qn::ItemGeometryRole:
            return "ItemGeometryRole";
        case Qn::ItemGeometryDeltaRole:
            return "ItemGeometryDeltaRole";
        case Qn::ItemCombinedGeometryRole:
            return "ItemCombinedGeometryRole";
        case Qn::ItemZoomRectRole:
            return "ItemZoomRectRole";
        case Qn::ItemPositionRole:
            return "ItemPositionRole";
        case Qn::ItemRotationRole:
            return "ItemRotationRole";
        case Qn::ItemFlipRole:
            return "ItemFlipRole";
        case Qn::ItemTimeRole:
            return "ItemTimeRole";
        case Qn::ItemPausedRole:
            return "ItemPausedRole";
        case Qn::ItemSpeedRole:
            return "ItemSpeedRole";
        case Qn::ItemCheckedButtonsRole:
            return "ItemCheckedButtonsRole";
        case Qn::ItemFrameDistinctionColorRole:
            return "ItemFrameDistinctionColorRole";
        case Qn::ItemSliderWindowRole:
            return "ItemSliderWindowRole";
        case Qn::ItemSliderSelectionRole:
            return "ItemSliderSelectionRole";
        case Qn::ItemImageEnhancementRole:
            return "ItemImageEnhancementRole";
        case Qn::ItemImageDewarpingRole:
            return "ItemImageDewarpingRole";
        case Qn::ItemHealthMonitoringButtonsRole:
            return "ItemHealthMonitoringButtonsRole";
        default:
            return "unknown role " + QString::number(role).toUtf8();
    }
}
#endif

QnVideoWallItemIndexList getIndices(const QnLayoutItemData& data)
{
    return qnResourceRuntimeDataManager->layoutItemData(data.uuid, Qn::VideoWallItemIndicesRole)
        .value<QnVideoWallItemIndexList>();
}

void setIndices(const QnLayoutItemData& data, const QnVideoWallItemIndexList& value)
{
    qnResourceRuntimeDataManager->setLayoutItemData(data.uuid, Qn::VideoWallItemIndicesRole, value);
}

void addItemToLayout(const QnLayoutResourcePtr &layout, const QnVideoWallItemIndexList& indices)
{
    if (indices.isEmpty())
        return;

    QnVideoWallItemIndex firstIdx = indices.first();
    if (!firstIdx.isValid())
        return;

    QList<int> screens = firstIdx.item().screenSnaps.screens().toList();
    if (screens.isEmpty())
        return;

    QnVideoWallPcData pc = firstIdx.videowall()->pcs()->getItem(firstIdx.item().pcUuid);
    if (screens.first() >= pc.screens.size())
        return;

    QRect geometry = pc.screens[screens.first()].layoutGeometry;
    if (geometry.isValid())
    {
        for (const QnLayoutItemData &item : layout->getItems())
        {
            if (!item.combinedGeometry.isValid())
                continue;
            if (!item.combinedGeometry.intersects(geometry))
                continue;
            geometry = QRect(); //invalidate selected geometry
            break;
        }
    }

    QnLayoutItemData itemData;
    itemData.uuid = QnUuid::createUuid();
    itemData.combinedGeometry = geometry;
    if (geometry.isValid())
        itemData.flags = Qn::Pinned;
    else
        itemData.flags = Qn::PendingGeometryAdjustment;
    itemData.resource.id = firstIdx.videowall()->getId();
    setIndices(itemData, indices);
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

    friend bool operator==(const ScreenWidgetKey &l, const ScreenWidgetKey &r)
    {
        return l.pcUuid == r.pcUuid && l.screens == r.screens;
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

const int identifyTimeout = 5000;
const int kIdentifyFontSize = 100;

const int cacheMessagesTimeoutMs = 500;

const qreal defaultReviewAR = 1920.0 / 1080.0;

const QnUuid uuidPoolBase("621992b6-5b8a-4197-af04-1657baab71f0");

// TODO: #GDM think about code duplication
QString toWindowsRegistryFormat(const QString& path)
{
    return L'"' + QDir::toNativeSeparators(path).toLower() + L'"';
}

QString binaryPath()
{
    const QFileInfo appLauncherFile = QnClientInstallationsManager::appLauncher();
    if (appLauncherFile.exists())
        return toWindowsRegistryFormat(appLauncherFile.canonicalFilePath());

    const QFileInfo miniLauncherFile = QnClientInstallationsManager::miniLauncher();
    if (miniLauncherFile.exists())
        return toWindowsRegistryFormat(miniLauncherFile.canonicalFilePath());

    return QString();
}

// TODO: #GDM #VW clean nonexistent videowalls sometimes
void setAutoRunEnabled(const QnUuid& videoWallUuid, bool value)
{
    auto commonModule = qnClientCoreModule->commonModule();
    const QString key = qApp->applicationName() + L' ' + videoWallUuid.toString();

    const QString path = binaryPath();
    if (path.isEmpty())
        value = false; // intentionally disable autorun if all goes bad

    QStringList arguments;
    arguments << lit("--videowall");
    arguments << videoWallUuid.toString();
    QUrl url = commonModule->currentUrl();
    url.setUserName(QString());
    url.setPassword(QString());
    arguments << lit("--auth");
    arguments << QString::fromUtf8(url.toEncoded());

    nx::vms::utils::setAutoRunEnabled(key, path + L' ' + arguments.join(L' '), value);
}

class QnVideowallReviewLayoutResource: public QnLayoutResource
{
public:
    QnVideowallReviewLayoutResource(const QnVideoWallResourcePtr &videowall):
        QnLayoutResource()
    {
        addFlags(Qn::local);
        setName(videowall->getName());
        setCellSpacing(0.1);
        setCellAspectRatio(defaultReviewAR);
        setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission));
        setData(Qn::VideoWallResourceRole, qVariantFromValue(videowall));

        connect(videowall.data(), &QnResource::nameChanged, this, [this](const QnResourcePtr &resource) { setName(resource->getName()); });
    }
};

auto offlineItemOnThisPc = []
    {
        QnUuid pcUuid = qnSettings->pcUuid();
        NX_ASSERT(!pcUuid.isNull(), "Invalid pc state.");
        return [pcUuid](const QnVideoWallItem& item)
            {
                return !pcUuid.isNull() && item.pcUuid == pcUuid && !item.runtimeStatus.online;
            };
    };

} /* anonymous namespace */

QnWorkbenchVideoWallHandler::QnWorkbenchVideoWallHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_licensesHelper(new QnVideoWallLicenseUsageHelper(commonModule(), this))
#ifdef _DEBUG
    /* Limit by reasonable size. */
    , m_uuidPool(new QnUuidPool(uuidPoolBase, 256))
#else
    , m_uuidPool(new QnUuidPool(uuidPoolBase, 16384))
#endif
{
    m_videoWallMode.active = qnRuntime->isVideoWallMode();
    m_videoWallMode.opening = false;
    m_videoWallMode.ready = false;
    m_controlMode.active = false;
    m_controlMode.sequence = 0;
    m_controlMode.cacheTimer = new QTimer(this);
    m_controlMode.cacheTimer->setInterval(cacheMessagesTimeoutMs);
    connect(m_controlMode.cacheTimer, &QTimer::timeout, this,
        &QnWorkbenchVideoWallHandler::at_controlModeCacheTimer_timeout);

    QnUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull())
    {
        pcUuid = QnUuid::createUuid();
        qnSettings->setPcUuid(pcUuid);
        qnSettings->save();
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
            if (info.data.peer.peerType == Qn::PT_VideowallClient)
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
            if (info.data.peer.peerType == Qn::PT_VideowallClient)
            {
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
            if (info.uuid == commonModule()->moduleGUID())
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
            if (info.uuid < commonModule()->moduleGUID())
            {
                setControlMode(false);
                showControlledByAnotherUserMessage();
            }

        });


    foreach(const QnPeerRuntimeInfo &info, runtimeInfoManager()->items()->getItems())
    {
        if (info.data.peer.peerType != Qn::PT_VideowallClient)
            continue;

        /* Master node, will run other clients and exit. */
        if (info.data.videoWallInstanceGuid.isNull())
            continue;

        setItemOnline(info.data.videoWallInstanceGuid, true);
    }

    const auto clientMessageProcessor = qnClientMessageProcessor;
    connect(clientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &QnWorkbenchVideoWallHandler::cleanupUnusedLayouts);

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

        connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
            &QnWorkbenchVideoWallHandler::at_display_widgetAdded);
        connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
            &QnWorkbenchVideoWallHandler::at_display_widgetAboutToBeRemoved);

        connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this,
            &QnWorkbenchVideoWallHandler::at_workbench_currentLayoutAboutToBeChanged);
        connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
            &QnWorkbenchVideoWallHandler::at_workbench_currentLayoutChanged);

        connect(navigator(), &QnWorkbenchNavigator::positionChanged, this,
            &QnWorkbenchVideoWallHandler::at_navigator_positionChanged);
        connect(navigator(), &QnWorkbenchNavigator::playingChanged, this,
            &QnWorkbenchVideoWallHandler::at_navigator_playingChanged);
        connect(navigator(), &QnWorkbenchNavigator::speedChanged, this,
            &QnWorkbenchVideoWallHandler::at_navigator_speedChanged);

        connect(context()->instance<QnWorkbenchStreamSynchronizer>(),
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

ec2::AbstractECConnectionPtr QnWorkbenchVideoWallHandler::connection2() const
{
    return commonModule()->ec2Connection();
}

void QnWorkbenchVideoWallHandler::resetLayout(const QnVideoWallItemIndexList &items, const QnLayoutResourcePtr &layout)
{
    if (items.isEmpty())
        return;

    layout->setCellSpacing(0.0);

    auto reset =
        [this](const QnVideoWallItemIndexList &items, const QnLayoutResourcePtr &layout)
        {
            updateItemsLayout(items, layout->getId());
        };

    const bool needToSave = !layout->isFile()
        && (layout->hasFlags(Qn::local) || snapshotManager()->isModified(layout));

    if (needToSave)
    {
        auto callback =
            [this, items, reset](bool success, const QnLayoutResourcePtr &layout)
            {
                if (!success)
                    showFailedToApplyChanges();
                else
                    reset(items, layout);
                updateMode();
            };
        snapshotManager()->save(layout, callback);
        propertyDictionary()->saveParamsAsync(layout->getId());
    }
    else
    {
        reset(items, layout);
    }
}

void QnWorkbenchVideoWallHandler::swapLayouts(const QnVideoWallItemIndex firstIndex, const QnLayoutResourcePtr &firstLayout, const QnVideoWallItemIndex &secondIndex, const QnLayoutResourcePtr &secondLayout)
{
    if (!firstIndex.isValid() || !secondIndex.isValid())
        return;

    QnLayoutResourceList unsavedLayouts;
    if (firstLayout && (firstLayout->hasFlags(Qn::local) || snapshotManager()->isModified(firstLayout)))
        unsavedLayouts << firstLayout;

    if (secondLayout && (secondLayout->hasFlags(Qn::local) || snapshotManager()->isModified(secondLayout)))
        unsavedLayouts << secondLayout;

    auto swap = [this](const QnVideoWallItemIndex firstIndex, const QnLayoutResourcePtr &firstLayout, const QnVideoWallItemIndex &secondIndex, const QnLayoutResourcePtr &secondLayout)
    {
        QnVideoWallItem firstItem = firstIndex.item();
        firstItem.layout = firstLayout ? firstLayout->getId() : QnUuid();
        firstIndex.videowall()->items()->updateItem(firstItem);

        QnVideoWallItem secondItem = secondIndex.item();
        secondItem.layout = secondLayout ? secondLayout->getId() : QnUuid();
        secondIndex.videowall()->items()->updateItem(secondItem);

        saveVideowalls(QSet<QnVideoWallResourcePtr>() << firstIndex.videowall() << secondIndex.videowall());
    };

    if (!unsavedLayouts.isEmpty())
    {

        auto callback = [this, firstIndex, firstLayout, secondIndex, secondLayout, swap](bool success, const QnLayoutResourcePtr &layout)
        {
            Q_UNUSED(layout);
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
            /* Avoiding double swap */
            // TODO: #GDM refactor it
            bool bothSuccess = true;
            nx::utils::Counter *counter = new nx::utils::Counter(2);
            connect(counter, &nx::utils::Counter::reachedZero, this, [callback, &bothSuccess, counter]()
            {
                callback(bothSuccess, QnLayoutResourcePtr());
                counter->deleteLater();
            });

            auto localCallback = [&bothSuccess, counter](bool success, const QnLayoutResourcePtr &layout)
            {
                Q_UNUSED(layout);
                bothSuccess &= success;
                counter->decrement();
            };
            for (const QnLayoutResourcePtr& layout : unsavedLayouts)
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

    saveVideowalls(videoWalls);
    cleanupUnusedLayouts();
}

bool QnWorkbenchVideoWallHandler::canStartVideowall(const QnVideoWallResourcePtr &videowall) const
{
    NX_ASSERT(videowall);
    if (!videowall)
        return false;

    return boost::algorithm::any_of(videowall->items()->getItems().values(), offlineItemOnThisPc());
}

void QnWorkbenchVideoWallHandler::switchToVideoWallMode(const QnVideoWallResourcePtr& videoWall)
{
    QList<QnVideoWallItem> items;
    boost::algorithm::copy_if(videoWall->items()->getItems().values(), std::back_inserter(items),
        offlineItemOnThisPc());

    NX_ASSERT(!items.isEmpty(), "Action condition must not allow us to get here.");
    if (items.isEmpty())
        return;

    bool closeCurrentInstance = false;
    if (!messages::Videowall::switchToVideoWallMode(mainWindow(), &closeCurrentInstance))
        return;

    if (closeCurrentInstance)
        closeInstanceDelayed();

    for (const auto& item: items)
    {
        QStringList arguments;
        arguments << lit("--videowall");
        arguments << videoWall->getId().toString();
        arguments << lit("--videowall-instance");
        arguments << item.uuid.toString();
        openNewWindow(arguments);
    }
}

void QnWorkbenchVideoWallHandler::openNewWindow(const QStringList &args)
{
    QStringList arguments = args;

    QUrl url = commonModule()->currentUrl();
    url.setUserName(QString());
    url.setPassword(QString());

    arguments << lit("--auth");
    arguments << QnStartupParameters::createAuthenticationString(url);

#ifdef SENDER_DEBUG
    qDebug() << "arguments" << arguments;
#endif
    UnityLauncherWorkaround::startDetached(qApp->applicationFilePath(), arguments);
}

void QnWorkbenchVideoWallHandler::openVideoWallItem(const QnVideoWallResourcePtr &videoWall)
{
    if (!videoWall)
    {
        NX_LOG("Warning: videowall not exists anymore, cannot open videowall item", cl_logERROR);
        closeInstanceDelayed();
        return;
    }

    QnVideoWallItem item = videoWall->items()->getItem(m_videoWallMode.instanceGuid);
    updateMainWindowGeometry(item.screenSnaps); // TODO: #GDM check if it is needed at all

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

    if (cached)
    {
        m_controlMode.cachedMessages << message;
        return;
    }

    QnVideoWallControlMessage localMessage(message);
    localMessage[sequenceKey] = QString::number(m_controlMode.sequence++);
    localMessage[pcUuidKey] = m_controlMode.pcUuid;

    ec2::ApiVideowallControlMessageData apiMessage;
    fromResourceToApi(localMessage, apiMessage);

#ifdef SENDER_DEBUG
    qDebug() << "SENDER: sending message" << message;
#endif
    for (const QnVideoWallItemIndex &index : targetList())
    {
        apiMessage.videowallGuid = index.videowall()->getId();
        apiMessage.instanceGuid = index.uuid();
        connection2()->getVideowallManager(Qn::kSystemAccess)->sendControlMessage(apiMessage, this, [] {});
    }
}

void QnWorkbenchVideoWallHandler::handleMessage(const QnVideoWallControlMessage &message, const QnUuid &controllerUuid, qint64 sequence)
{
#ifdef RECEIVER_DEBUG
    qDebug() << "RECEIVER: handling message" << message;
#endif

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
                workbench()->setItem(role, NULL);
            else
                workbench()->setItem(role, workbench()->currentLayout()->item(guid));
            break;
        }
        case QnVideoWallControlMessage::LayoutDataChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            int role = message[roleKey].toInt();
            switch (role)
            {
                case Qn::LayoutCellSpacingRole:
                {
                    auto data = QJson::deserialized<qreal>(value);
                    workbench()->currentLayout()->setData(role, data);
                    break;
                }
                case Qn::LayoutCellAspectRatioRole:
                {
                    qreal data;
                    QJson::deserialize(value, &data);
                    workbench()->currentLayout()->setData(role, data);
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

            // TODO: #GDM Allow dropping of the local files outside of media folders.
            QString resourceUid = message[resourceKey];
            const auto resource = resourcePool()->getResourceByUniqueId(resourceUid);
            if (!resource)
                return;

            QRect geometry = QJson::deserialized<QRect>(message[geometryKey].toUtf8());
            QRectF zoomRect = QJson::deserialized<QRectF>(message[zoomRectKey].toUtf8());
            qreal rotation = QJson::deserialized<qreal>(message[rotationKey].toUtf8());
            int checkedButtons = QJson::deserialized<int>(message[checkedButtonsKey].toUtf8());
            QnWorkbenchItem* item = new QnWorkbenchItem(resource, uuid, workbench()->currentLayout());
            item->setGeometry(geometry);
            item->setZoomRect(zoomRect);
            item->setRotation(rotation);
            item->setFlag(Qn::PendingGeometryAdjustment);
            item->setData(Qn::ItemCheckedButtonsRole, checkedButtons);
            workbench()->currentLayout()->addItem(item);

#ifdef RECEIVER_DEBUG
            qDebug() << "RECEIVER: Item" << uuid << "added to" << geometry;
#endif
            break;
        }
        case QnVideoWallControlMessage::LayoutItemRemoved:
        {
            QnUuid uuid = QnUuid(message[uuidKey]);
            if (QnWorkbenchItem* item = workbench()->currentLayout()->item(uuid))
                workbench()->currentLayout()->removeItem(item);
            break;
        }
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
#ifdef RECEIVER_DEBUG
                    qDebug() << "RECEIVER: Item" << uuid << "geometry changed to" << data;
#endif
                    break;
                }
                case Qn::ItemGeometryDeltaRole:
                case Qn::ItemCombinedGeometryRole:
                case Qn::ItemZoomRectRole:
                {
                    QRectF data = QJson::deserialized<QRectF>(value);
                    item->setData(role, data);
#ifdef RECEIVER_DEBUG
                    qDebug() << "RECEIVER: Item" << uuid << debugRole(role) << "changed to" << data;
#endif
                    break;
                }
                case Qn::ItemPositionRole:
                {
                    QPointF data = QJson::deserialized<QPointF>(value);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemRotationRole:
                case Qn::ItemSpeedRole:
                {
                    qreal data;
                    QJson::deserialize(value, &data);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemFlipRole:
                case Qn::ItemPausedRole:
                {
                    bool data;
                    QJson::deserialize(value, &data);
                    item->setData(role, data);
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
                    break;
                }
                case Qn::ItemImageEnhancementRole:
                {
                    ImageCorrectionParams data = QJson::deserialized<ImageCorrectionParams>(value);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemImageDewarpingRole:
                {
                    QnItemDewarpingParams data = QJson::deserialized<QnItemDewarpingParams>(value);
                    item->setData(role, data);
                    break;
                }
                case Qn::ItemHealthMonitoringButtonsRole:
                {
                    QnServerResourceWidget::HealthMonitoringButtons data = QJson::deserialized<QnServerResourceWidget::HealthMonitoringButtons>(value);
                    item->setData(role, data);
                    break;
                }

                default:
                    break;
            }

            break;
        }
        case QnVideoWallControlMessage::ZoomLinkAdded:
        {
            QnWorkbenchItem* item = workbench()->currentLayout()->item(QnUuid(message[uuidKey]));
            QnWorkbenchItem* zoomTargetItem = workbench()->currentLayout()->item(QnUuid(message[zoomUuidKey]));
#ifdef RECEIVER_DEBUG
            qDebug() << "RECEIVER: ZoomLinkAdded message" << message << item << zoomTargetItem;
#endif
            if (!item || !zoomTargetItem)
                return;
            workbench()->currentLayout()->addZoomLink(item, zoomTargetItem);
            break;
        }
        case QnVideoWallControlMessage::ZoomLinkRemoved:
        {
            QnWorkbenchItem* item = workbench()->currentLayout()->item(QnUuid(message[uuidKey]));
            QnWorkbenchItem* zoomTargetItem = workbench()->currentLayout()->item(QnUuid(message[zoomUuidKey]));
#ifdef RECEIVER_DEBUG
            qDebug() << "RECEIVER: ZoomLinkRemoved message" << message << item << zoomTargetItem;
#endif
            if (!item || !zoomTargetItem)
                return;
            workbench()->currentLayout()->removeZoomLink(item, zoomTargetItem);
            break;
        }
        case QnVideoWallControlMessage::NavigatorPositionChanged:
        {
            navigator()->setPosition(message[positionKey].toLongLong());
            break;
        }
	    case QnVideoWallControlMessage::NavigatorPlayingChanged:
	    {
	        navigator()->setPlaying(QnLexical::deserialized<bool>(message[valueKey]));
	        break;
	    }
        case QnVideoWallControlMessage::NavigatorSpeedChanged:
        {
            navigator()->setSpeed(message[speedKey].toDouble());
	        if (message.contains(positionKey))
	            navigator()->setPosition(message[positionKey].toLongLong());
            break;
        }
        case QnVideoWallControlMessage::SynchronizationChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            QnStreamSynchronizationState state = QJson::deserialized<QnStreamSynchronizationState>(value);
            context()->instance<QnWorkbenchStreamSynchronizer>()->setState(state);
            break;
        }
        case QnVideoWallControlMessage::MotionSelectionChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            QList<QRegion> regions = QJson::deserialized<QList<QRegion> >(value);
            QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(QnUuid(message[uuidKey])));
            if (widget)
                widget->setMotionSelection(regions);
            break;
        }
        case QnVideoWallControlMessage::MediaDewarpingParamsChanged:
        {
            QByteArray value = message[valueKey].toUtf8();
            QnMediaDewarpingParams params = QJson::deserialized<QnMediaDewarpingParams>(value);
            QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(QnUuid(message[uuidKey])));
            if (widget)
                widget->setDewarpingParams(params);
            break;
        }
        case QnVideoWallControlMessage::Identify:
        {
            QnVideoWallResourcePtr videoWall = resourcePool()->getResourceById<QnVideoWallResource>(m_videoWallMode.guid);
            if (!videoWall)
                return;

            QnVideoWallItem data = videoWall->items()->getItem(m_videoWallMode.instanceGuid);
            if (!data.name.isEmpty())
            {
                auto messageBox = QnGraphicsMessageBox::information(data.name, identifyTimeout);
                if (messageBox)
                {
                    QFont f = messageBox->font();
                    f.setPixelSize(kIdentifyFontSize);
                    messageBox->setFont(f);
                }
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

            QnLayoutItemIndexList layoutItems;
            for (const auto& id: items)
                layoutItems.push_back(QnLayoutItemIndex(layout, id));

            ui::action::Parameters parameters(layoutItems);
            parameters.setArgument(Qn::ResolutionModeRole, mode);
            menu()->trigger(ui::action::RadassAction, parameters);
            break;
        }
        default:
            break;
    }
}

void QnWorkbenchVideoWallHandler::storeMessage(const QnVideoWallControlMessage &message, const QnUuid &controllerUuid, qint64 sequence)
{
    m_videoWallMode.storedMessages[controllerUuid][sequence] = message;
#ifdef RECEIVER_DEBUG
    qDebug() << "RECEIVER:" << "store message" << message;
#endif
}

void QnWorkbenchVideoWallHandler::restoreMessages(const QnUuid &controllerUuid, qint64 sequence)
{
    StoredMessagesHash &stored = m_videoWallMode.storedMessages[controllerUuid];
    while (stored.contains(sequence + 1))
    {
        QnVideoWallControlMessage message = stored.take(++sequence);
#ifdef RECEIVER_DEBUG
        qDebug() << "RECEIVER:" << "restored message" << message;
#endif
        handleMessage(message, controllerUuid, sequence);
    }
}


bool QnWorkbenchVideoWallHandler::canStartControlMode() const
{
    if (!m_licensesHelper->isValid(Qn::LC_VideoWall))
    {
        QnMessageBox::warning(mainWindow(),
            tr("Video Wall license required"),
            tr("To enable this feature, please activate a Video Wall license."));
        return false;
    }


    QnVideoWallLicenseUsageProposer proposer(m_licensesHelper, 0, 1);
    if (!validateLicenses(tr("Activate one more license to start Video Wall control session.")))
        return false;

    auto layout = workbench()->currentLayout()->resource();
    if (!layout)
        return false;

    foreach(const QnPeerRuntimeInfo &info, runtimeInfoManager()->items()->getItems())
    {
        if (info.data.videoWallControlSession != layout->getId())
            continue;

        /* Ignore our control session. */
        if (info.uuid == runtimeInfoManager()->localInfo().uuid)
            continue;

        showControlledByAnotherUserMessage();

        return false;
    }

    return true;
}

void QnWorkbenchVideoWallHandler::showFailedToApplyChanges() const
{
    QnMessageBox::critical(mainWindow(), tr("Failed to apply changes"));
}

void QnWorkbenchVideoWallHandler::showControlledByAnotherUserMessage() const
{
    QnMessageBox::warning(mainWindow(),
        tr("Screen is being controlled by another user"),
        tr("Control session cannot be started."));
}

void QnWorkbenchVideoWallHandler::setControlMode(bool active)
{
    auto layoutResource = workbench()->currentLayout()->resource();
    NX_ASSERT(layoutResource || !active);

    if (active && !canStartControlMode())
        return;

    if (m_controlMode.active == active)
        return;

    m_controlMode.cachedMessages.clear();

    QnWorkbenchLayout* layout = workbench()->currentLayout();
    if (active)
    {
        connect(action(action::RadassAction), &QAction::triggered, this,
            &QnWorkbenchVideoWallHandler::at_radassAction_triggered);

        connect(workbench(), &QnWorkbench::itemChanged, this, &QnWorkbenchVideoWallHandler::at_workbench_itemChanged);
        connect(layout, &QnWorkbenchLayout::itemAdded, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode);
        connect(layout, &QnWorkbenchLayout::itemRemoved, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode);
        connect(layout, &QnWorkbenchLayout::zoomLinkAdded, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded);
        connect(layout, &QnWorkbenchLayout::zoomLinkRemoved, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved);

        foreach(QnWorkbenchItem* item, layout->items())
        {
            connect(item, &QnWorkbenchItem::dataChanged, this, &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);
        }

        m_controlMode.active = active;
        m_controlMode.cacheTimer->start();
        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStarted));  // TODO: #GDM #VW start control when item goes online

        QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
        localInfo.data.videoWallControlSession = layoutResource->getId();
        runtimeInfoManager()->updateLocalItem(localInfo);

        setItemControlledBy(localInfo.data.videoWallControlSession, localInfo.uuid, true);
    }
    else
    {
        action(action::RadassAction)->disconnect(this);

        disconnect(workbench(), &QnWorkbench::itemChanged, this, &QnWorkbenchVideoWallHandler::at_workbench_itemChanged);
        disconnect(layout, &QnWorkbenchLayout::itemAdded, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode);
        disconnect(layout, &QnWorkbenchLayout::itemRemoved, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode);
        disconnect(layout, &QnWorkbenchLayout::zoomLinkAdded, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded);
        disconnect(layout, &QnWorkbenchLayout::zoomLinkRemoved, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved);

        foreach(QnWorkbenchItem* item, layout->items())
        {
            disconnect(item, &QnWorkbenchItem::dataChanged, this, &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);
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
            && (item.runtimeStatus.controlledBy.isNull() || item.runtimeStatus.controlledBy == commonModule()->moduleGUID())
            && layout->resource()
            && item.layout == layout->resource()->getId()
            )
            control = true;
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

    QnVideoWallResourcePtr videoWall = resourcePool()->getResourceById<QnVideoWallResource>(m_videoWallMode.guid);
    if (!videoWall || videoWall->items()->getItems().isEmpty())
    {
        QString message = videoWall
            ? lit("Warning: videowall %1 is empty, cannot start videowall on this pc")
            : lit("Warning: videowall %1 not exists, cannot start videowall on this pc");

        NX_LOG(message.arg(m_videoWallMode.guid.toString()), cl_logERROR);

        setAutoRunEnabled(m_videoWallMode.guid, false);
        closeInstanceDelayed();
        return;
    }

    QnUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull())
    {
        NX_LOG(lit("Warning: pc UUID is null, cannot start videowall %1 on this pc")
            .arg(m_videoWallMode.guid.toString()), cl_logERROR);

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

            QStringList arguments;
            arguments << QLatin1String("--videowall");
            arguments << m_videoWallMode.guid.toString();
            arguments << QLatin1String("--videowall-instance");
            arguments << item.uuid.toString();
            openNewWindow(arguments);
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

    for (const QnVideoWallResourcePtr& videoWall : resourcePool()->getResources<QnVideoWallResource>())
    {
        for (const QnVideoWallItem &item : videoWall->items()->getItems())
        {
            if (item.layout == currentId && item.runtimeStatus.online)
                indices << QnVideoWallItemIndex(videoWall, item.uuid);
        }
    }

    return indices;
}

QnLayoutResourcePtr QnWorkbenchVideoWallHandler::constructLayout(const QnResourceList &resources) const
{
    QSet<QnResourcePtr> filtered;
    QnVirtualCameraResourceList cameras;
    QMap<qreal, int> aspectRatios;
    qreal defaultAr = qnGlobals->defaultLayoutCellAspectRatio();

    auto addToFiltered = [&](const QnResourcePtr &resource)
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
        if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        {
            for (auto resource: layout->layoutResources())
                addToFiltered(resource);
        }
        else
        {
            addToFiltered(resource);
        }
    }

    /* If we have provided the only layout, copy from it as much as possible. */
    QnLayoutResourcePtr sourceLayout = resources.size() == 1
        ? resources.first().dynamicCast<QnLayoutResource>()
        : QnLayoutResourcePtr();

    QnLayoutResourcePtr layout = sourceLayout ?
        sourceLayout->clone() :
        QnLayoutResourcePtr(new QnLayoutResource());

    layout->setId(m_uuidPool->getFreeId());
    layout->addFlags(Qn::local); // TODO: #Elric #EC2

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
            QnLayoutItemData item;
            item.flags = Qn::Pinned;
            item.uuid = QnUuid::createUuid();
            item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
            item.resource.id = resource->getId();
            if (resource->hasFlags(Qn::local_media))
                item.resource.uniqueId = resource->getUniqueId();
            layout->addItem(item);
            i++;
        }
    }

    return layout;
}

void QnWorkbenchVideoWallHandler::cleanupUnusedLayouts()
{
    QnLayoutResourceList layoutsToDelete;

    for (const auto& videowall: resourcePool()->getResources<QnVideoWallResource>())
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
        QnLayoutResourceList unused = resourcePool()->getResourcesByParentId(videoWallId)
            .filtered<QnLayoutResource>(
            [used](const QnLayoutResourcePtr& layout)
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
        if (!menu()->triggerIfPossible(action::RemoveFromServerAction, layout))
            resourcePool()->removeResource(layout);
    }
}

/*------------------------------------ HANDLERS ------------------------------------------*/

void QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered()
{

    QnLicenseListHelper licenseList(licensePool()->getLicenses());
    if (licenseList.totalLicenseByType(Qn::LC_VideoWall, licensePool()->validator()) == 0)
    {
        QnMessageBox::warning(mainWindow(),
            tr("Video Wall license required"),
            tr("To enable Video Wall, please activate a Video Wall license."));
        return;
    } // TODO: #GDM add "Licenses" button

    QStringList usedNames;
    foreach(const QnResourcePtr &resource, resourcePool()->getResourcesWithFlag(Qn::videowall))
        usedNames << resource->getName().trimmed().toLower();

    // TODO: #GDM #VW refactor to corresponding dialog
    QString proposedName = nx::utils::generateUniqueString(usedNames, tr("Video Wall"), tr("Video Wall %1"));

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
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
            messages::Videowall::anotherVideoWall(mainWindow());
            continue;
        }

        break;
    };

    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setId(QnUuid::createUuid());
    videoWall->setName(proposedName);

    // No need to backup newly created videowall.
    auto applyChangesFunction = QnResourcesChangesManager::VideoWallChangesFunction();
    auto callbackFunction =
        [this](bool success, const QnVideoWallResourcePtr& videoWall)
        {
            // Cannot capture the resource directly because real resource pointer may differ if the
            // transaction is received before the request callback.
            NX_EXPECT(videoWall);
            if (success && videoWall)
            {
                menu()->trigger(action::SelectNewItemAction, videoWall);
                menu()->trigger(action::OpenVideoWallReviewAction, videoWall);
            }
        };

    qnResourcesChangesManager->saveVideoWall(videoWall, applyChangesFunction, callbackFunction);
}

void QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered()
{
    if (!context()->user())
        return;

    const auto parameters = menu()->currentParameters(sender());
    QnVideoWallResourcePtr videoWall = parameters.resource().dynamicCast<QnVideoWallResource>();
    if (videoWall.isNull())
        return;

    QScopedPointer<QnAttachToVideowallDialog> dialog(new QnAttachToVideowallDialog(mainWindow()));
    dialog->loadFromResource(videoWall);
    if (!dialog->exec())
        return;

    qnResourcesChangesManager->saveVideoWall(videoWall, [d = dialog.data()](const QnVideoWallResourcePtr &videoWall)
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
        if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(existingItem.layout))
        {
            auto removedResources = resourcePool()->getResources(layout->layoutResourceIds());
            if (!confirmRemoveResourcesFromLayout(layout, removedResources))
                break;
        }

        existingItem.layout = QnUuid();
        index.videowall()->items()->updateItem(existingItem);
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls);
    cleanupUnusedLayouts();
}

void QnWorkbenchVideoWallHandler::at_deleteVideoWallItemAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVideoWallItemIndexList items = parameters.videoWallItems();

    QnResourceList resources;
    for (const auto& index: items)
    {
        if (!index.isValid())
            continue;

        QnResourcePtr proxyResource(new QnResource());
        proxyResource->setId(index.uuid());
        proxyResource->setName(index.item().name);
        qnResIconCache->setKey(proxyResource, QnResourceIconCache::VideoWallItem);
        resources.append(proxyResource);
    }

    QnMessageBox messageBox(QnMessageBoxIcon::Question,
        tr("Delete %n items?", "", resources.size()), QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindow());
    messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));
    auto result = messageBox.exec();

    if (result == QDialogButtonBox::Cancel)
        return;

    for (auto resource : resources)
        qnResIconCache->clearKey(resource);

    QSet<QnVideoWallResourcePtr> videoWalls;
    for (const auto& index: items)
    {
        if (!index.isValid())
            continue;

        index.videowall()->items()->removeItem(index.uuid());
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls, true);
}

void QnWorkbenchVideoWallHandler::at_startVideoWallAction_triggered()
{
    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (videoWall.isNull())
        return;

    if (!validateLicenses(tr("Activate one more license to start Video Wall.")))
        return;

    switchToVideoWallMode(videoWall);
}

void QnWorkbenchVideoWallHandler::at_stopVideoWallAction_triggered()
{
    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (videoWall.isNull())
        return;

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Stop Video Wall?"),
        tr("To start it again, you should have physical access to its computer."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindow());

    dialog.addButton(tr("Stop"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    ec2::ApiVideowallControlMessageData message;
    message.operation = QnVideoWallControlMessage::Exit;
    message.videowallGuid = videoWall->getId();

    for (const QnVideoWallItem &item : videoWall->items()->getItems())
    {
        message.instanceGuid = item.uuid;
        connection2()->getVideowallManager(Qn::kSystemAccess)->sendControlMessage(message, this, [] {});
    }
}

void QnWorkbenchVideoWallHandler::at_delayedOpenVideoWallItemAction_triggered()
{
    m_videoWallMode.guid = menu()->currentParameters(sender()).argument<QnUuid>(Qn::VideoWallGuidRole);
    m_videoWallMode.instanceGuid = menu()->currentParameters(sender()).argument<QnUuid>(Qn::VideoWallItemGuidRole);
    m_videoWallMode.opening = true;
    submitDelayedItemOpen();
}

void QnWorkbenchVideoWallHandler::at_renameAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();

    bool valid = false;
    switch (nodeType)
    {
        case Qn::VideoWallItemNode:
            valid = parameters.videoWallItems().size() == 1 && parameters.videoWallItems().first().isValid();
            break;
        case Qn::VideoWallMatrixNode:
            valid = parameters.videoWallMatrices().size() == 1 && parameters.videoWallMatrices().first().isValid();
            break;
        default:
            // do nothing
            break;
    }

    NX_ASSERT(valid, Q_FUNC_INFO, "Data should correspond to action profile.");
    if (!valid)
        return;

    QString oldName;
    switch (nodeType)
    {
        case Qn::VideoWallItemNode:
            oldName = parameters.videoWallItems().first().item().name;
            break;
        case Qn::VideoWallMatrixNode:
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
        case Qn::VideoWallItemNode:
        {
            QnVideoWallItemIndex index = parameters.videoWallItems().first();
            QnVideoWallItem existingItem = index.item();
            existingItem.name = name;
            index.videowall()->items()->updateItem(existingItem);
            saveVideowall(index.videowall());
        }
        break;

        case Qn::VideoWallMatrixNode:
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
    const auto parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList indices = parameters.videoWallItems();
    if (indices.isEmpty())
    {
        foreach(QnVideoWallResourcePtr videoWall, parameters.resources().filtered<QnVideoWallResource>())
        {
            if (!videoWall)
                continue;
            foreach(const QnVideoWallItem &item, videoWall->items()->getItems())
            {
                indices << QnVideoWallItemIndex(videoWall, item.uuid);
            }
        }
    }

    ec2::ApiVideowallControlMessageData message;
    message.operation = QnVideoWallControlMessage::Identify;
    for (const QnVideoWallItemIndex &item : indices)
    {
        if (!item.isValid())
            continue;

        message.videowallGuid = item.videowall()->getId();
        message.instanceGuid = item.uuid();
        connection2()->getVideowallManager(Qn::kSystemAccess)->sendControlMessage(message, this, [] {});
    }
}

void QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnWorkbenchLayout *layout = NULL;

    QnVideoWallItemIndexList indices = parameters.videoWallItems();

    foreach(QnVideoWallItemIndex index, indices)
    {
        if (!index.isValid())
            continue;
        QnVideoWallItem item = index.item();

        QnLayoutResourcePtr layoutResource = item.layout.isNull()
            ? QnLayoutResourcePtr()
            : resourcePool()->getResourceById<QnLayoutResource>(item.layout);

        if (!layoutResource)
        {
            layoutResource = constructLayout(QnResourceList());
            layoutResource->setParentId(index.videowall()->getId());
            layoutResource->setName(index.item().name);
            resourcePool()->addResource(layoutResource);
            resetLayout(QnVideoWallItemIndexList() << index, layoutResource);
        }

        layout = QnWorkbenchLayout::instance(layoutResource);
        if (!layout)
        {
            layout = qnWorkbenchLayoutsFactory->create(layoutResource, workbench());
            workbench()->addLayout(layout);
        }
        layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(item.uuid));
        layout->notifyTitleChanged();
    }

    if (layout)
        workbench()->setCurrentLayout(layout);
}

void QnWorkbenchVideoWallHandler::at_openVideoWallReviewAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto videoWall = parameters.resource().dynamicCast<QnVideoWallResource>();
    NX_EXPECT(videoWall);
    if (!videoWall)
        return;

    const auto existingLayout = QnWorkbenchLayout::instance(videoWall);
    if (existingLayout)
    {
        workbench()->setCurrentLayout(existingLayout);
        return;
    }

    /* Construct and add a new layout. */
    QnLayoutResourcePtr layout(new QnVideowallReviewLayoutResource(videoWall));
    layout->setId(QnUuid::createUuid());
    if (context()->user())
        layout->setParentId(videoWall->getId());
    if (accessController()->hasGlobalPermission(Qn::GlobalControlVideoWallPermission))
        layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission));

    QMap<ScreenWidgetKey, QnVideoWallItemIndexList> itemGroups;

    for (const auto& item: videoWall->items()->getItems())
    {
        ScreenWidgetKey key(item.pcUuid, item.screenSnaps.screens());
        itemGroups[key].append(QnVideoWallItemIndex(videoWall, item.uuid));
    }

    for (const auto& indices: itemGroups)
        addItemToLayout(layout, indices);

    resourcePool()->addResource(layout);

    menu()->trigger(action::OpenInNewTabAction, layout);

    // New layout should not be marked as changed, make sure it will be done after layout opening.
    executeDelayedParented(
        [this, videoWall, layout]
        {
            saveVideowallAndReviewLayout(videoWall, layout);
        },
        kDefaultDelay,
        this);
}

void QnWorkbenchVideoWallHandler::at_saveCurrentVideoWallReviewAction_triggered()
{
    QnWorkbenchLayout* layout = workbench()->currentLayout();
    QnVideoWallResourcePtr videowall = layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!videowall)
        return;
    saveVideowallAndReviewLayout(videowall, layout->resource());
}

void QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVideoWallResourcePtr videowall = parameters.resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;
    saveVideowallAndReviewLayout(videowall);
}

void QnWorkbenchVideoWallHandler::at_dropOnVideoWallItemAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnUuid targetUuid = parameters.argument(Qn::VideoWallItemGuidRole).value<QnUuid>();
    QnVideoWallItemIndex targetIndex = resourcePool()->getVideoWallItemByUuid(targetUuid);
    if (targetIndex.isNull())
        return;

    /* Layout that is currently on the drop-target item. */
    QnLayoutResourcePtr currentLayout = resourcePool()->getResourceById<QnLayoutResource>(targetIndex.item().layout);

    Qt::KeyboardModifiers keyboardModifiers = parameters.argument<Qt::KeyboardModifiers>(Qn::KeyboardModifiersRole);

    /* Case of dropping from one screen to another. */
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();
    QnVideoWallItemIndex sourceIndex;

    /* Resources that are being dropped (if dropped from tree or other screen). */
    QnResourceList targetResources;

    if (!videoWallItems.isEmpty())
    {
        for (const QnVideoWallItemIndex &index : videoWallItems)
        {
            if (!index.isValid())
                continue;

            if (QnLayoutResourcePtr layout = resourcePool()->getResourceById<QnLayoutResource>(index.item().layout))
                targetResources << layout;
        }

        /* Dragging single videowall item causing swap (if Shift is not pressed). */
        if (videoWallItems.size() == 1 && !videoWallItems.first().isNull() && !(Qt::ShiftModifier & keyboardModifiers))
            sourceIndex = videoWallItems.first();
    }
    else
    {
        targetResources = parameters.resources();
    }

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

    bool hasDesktopCamera = boost::algorithm::any_of(targetResources, [](const QnResourcePtr &resource) { return resource->hasFlags(Qn::desktop_camera); });

    if (currentLayout)
        hasDesktopCamera |= boost::algorithm::any_of(currentLayout->getItems().values(), [this](const QnLayoutItemData &item)
    {
        QnResourcePtr childResource = resourcePool()->getResourceById(item.resource.id);
        return childResource && childResource->hasFlags(Qn::desktop_camera);
    });

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

    QnLayoutResourcePtr targetLayout;
    if (targetResources.size() == 1)
    {
        auto layout = targetResources.first().dynamicCast<QnLayoutResource>();
        if (layout)
        {
            if (layout->getParentId() == videoWallId || layout->hasFlags(Qn::exported_layout))
                targetLayout = layout;
        }
    }

    if (!targetLayout)
    {
        targetLayout = constructLayout(targetResources);
        targetLayout->setParentId(videoWallId);
    }


    /* User can occasionally remove own access to cameras by dropping something on videowall. */
    if (dropAction == Action::SetAction && currentLayout)
    {
        const auto oldResources = currentLayout->layoutResourceIds();
        const auto newResources = targetLayout->layoutResourceIds();

        const auto removedResources = resourcePool()->getResources(oldResources - newResources);
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
            break;
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

    const auto desktopCameraId = QnDesktopResource::calculateUniqueId(
        commonModule()->moduleGUID(), user->getId());

    const auto desktopCamera = resourcePool()->getResourceByUniqueId<QnVirtualCameraResource>(
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
    QnVideoWallResourcePtr videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    QScopedPointer<QnVideowallSettingsDialog> dialog(new QnVideowallSettingsDialog(mainWindow()));
    dialog->loadFromResource(videowall);

    if (!dialog->exec())
        return;

    dialog->submitToResource(videowall);
    saveVideowall(videowall);
}

void QnWorkbenchVideoWallHandler::at_saveVideowallMatrixAction_triggered()
{
    QnVideoWallResourcePtr videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
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
        QnMessageBox::warning(mainWindow(), tr("Cannot save empty matrix"));
        return;
    }

    videowall->matrices()->addItem(matrix);
    saveVideowall(videowall);
}


void QnWorkbenchVideoWallHandler::at_loadVideowallMatrixAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnVideoWallMatrixIndexList matrices = parameters.videoWallMatrices();
    if (matrices.size() != 1)
        return;

    QnVideoWallMatrixIndex index = matrices.first();
    if (index.isNull())
        return;

    QnVideoWallResourcePtr videowall = index.videowall();
    if (!videowall->matrices()->hasItem(index.uuid()))
        return;

    QnVideoWallMatrix matrix = videowall->matrices()->getItem(index.uuid());

    QnVideoWallItemMap items = videowall->items()->getItems();

    bool hasChanges = false;
    foreach(QnVideoWallItem item, items)
    {
        if (!matrix.layoutByItem.contains(item.uuid))
            continue;

        QnUuid layoutUuid = matrix.layoutByItem[item.uuid];
        if (!layoutUuid.isNull() && !resourcePool()->getResourceById(layoutUuid))
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
    QnVideoWallMatrixIndexList matrices = parameters.videoWallMatrices();

    QnResourceList resources;
    for (const auto& matrix: matrices)
    {
        if (!matrix.videowall() || !matrix.videowall()->matrices()->hasItem(matrix.uuid()))
            continue;

        QnResourcePtr proxyResource(new QnResource());
        proxyResource->setId(matrix.uuid());
        proxyResource->setName(matrix.videowall()->matrices()->getItem(matrix.uuid()).name);
        qnResIconCache->setKey(proxyResource, QnResourceIconCache::VideoWallMatrix);
        resources.append(proxyResource);
    }

    QnMessageBox messageBox(QnMessageBoxIcon::Question,
        tr("Delete %n matrices?", "", resources.size()), QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindow());

    messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    messageBox.addCustomWidget(new QnResourceListView(resources, &messageBox));

    if (messageBox.exec() == QDialogButtonBox::Cancel)
        return;

    for (auto resource: resources)
        qnResIconCache->clearKey(resource);

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

    QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (!videoWall)
        return;

    connect(videoWall, &QnVideoWallResource::autorunChanged, this,
        [this](const QnResourcePtr &resource)
        {
            QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
            if (!videoWall || !videoWall->pcs()->hasItem(qnSettings->pcUuid()))
                return;
            setAutoRunEnabled(videoWall->getId(), videoWall->isAutorun());
        });

    connect(videoWall, &QnVideoWallResource::pcAdded, this,
        [this](const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
        {
            if (pc.uuid != qnSettings->pcUuid())
                return;
            setAutoRunEnabled(videoWall->getId(), videoWall->isAutorun());
        });

    connect(videoWall, &QnVideoWallResource::pcRemoved, this,
        [this](const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
        {
            if (pc.uuid != qnSettings->pcUuid())
                return;
            setAutoRunEnabled(videoWall->getId(), false);
        });

    if (m_videoWallMode.active)
    {
        if (videoWall->getId() == m_videoWallMode.guid)
        {
            connect(videoWall, &QnVideoWallResource::itemChanged, this, &QnWorkbenchVideoWallHandler::at_videoWall_itemChanged_activeMode);
            connect(videoWall, &QnVideoWallResource::itemRemoved, this, &QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode);
            setAutoRunEnabled(videoWall->getId(), videoWall->isAutorun());
            if (m_videoWallMode.ready)
                openVideoWallItem(videoWall);
        }
    }
    else
    {
        connect(videoWall, &QnVideoWallResource::pcAdded,       this, &QnWorkbenchVideoWallHandler::at_videoWall_pcAdded);
        connect(videoWall, &QnVideoWallResource::pcChanged,     this, &QnWorkbenchVideoWallHandler::at_videoWall_pcChanged);
        connect(videoWall, &QnVideoWallResource::pcRemoved,     this, &QnWorkbenchVideoWallHandler::at_videoWall_pcRemoved);
        connect(videoWall, &QnVideoWallResource::itemAdded,     this, &QnWorkbenchVideoWallHandler::at_videoWall_itemAdded);
        connect(videoWall, &QnVideoWallResource::itemChanged,   this, &QnWorkbenchVideoWallHandler::at_videoWall_itemChanged);
        connect(videoWall, &QnVideoWallResource::itemRemoved,   this, &QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved);

        foreach (const QnVideoWallItem &item, videoWall->items()->getItems())
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

    disconnect(videoWall, nullptr, this, nullptr);
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

void QnWorkbenchVideoWallHandler::at_videoWall_pcAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
{
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    foreach(const QnVideoWallItem &item, videoWall->items()->getItems())
    {
        if (item.pcUuid != pc.uuid)
            continue;
        at_videoWall_itemAdded(videoWall, item);
    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
{
    // TODO: #GDM #VW implement screen size changes handling
    QN_UNUSED(videoWall);
    QN_UNUSED(pc);
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc)
{
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    QList<QnWorkbenchItem*> itemsToDelete;
    foreach(QnWorkbenchItem *workbenchItem, layout->items())
    {
        QnLayoutItemData data = workbenchItem->data();
        auto indices = getIndices(data);
        if (indices.isEmpty())
            continue;
        if (indices.first().item().pcUuid != pc.uuid)
            continue;
        itemsToDelete << workbenchItem;
    }

    foreach(QnWorkbenchItem *workbenchItem, itemsToDelete)
        layout->removeItem(workbenchItem);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item)
{

    QnVideoWallItem updatedItem(item);
    bool hasChanges = false;

    foreach(const QnPeerRuntimeInfo &info, runtimeInfoManager()->items()->getItems())
    {
        if (info.data.videoWallInstanceGuid == item.uuid)
        {
            hasChanges |= !updatedItem.runtimeStatus.online;
            updatedItem.runtimeStatus.online = true;
        }

        if (info.data.videoWallControlSession == item.layout && !info.data.videoWallControlSession.isNull())
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

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged(const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& item)
{
    // TODO: #GDM #VW implement screen size changes handling

    /* Check if item's layout was changed. */
    QnUuid controller = getLayoutController(item.layout);
    if (item.runtimeStatus.controlledBy != controller)
    {
        QnVideoWallItem updatedItem(item);
        updatedItem.runtimeStatus.controlledBy = controller;
        videoWall->items()->updateItem(updatedItem);
        return; // we will re-enter the handler
    }

    updateControlLayout(videoWall, item, ItemAction::Changed);
    updateReviewLayout(videoWall, item, ItemAction::Changed);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item)
{
    updateControlLayout(videoWall, item, ItemAction::Removed);
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

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item)
{
    if (videoWall->getId() != m_videoWallMode.guid || item.uuid != m_videoWallMode.instanceGuid)
        return;
    closeInstanceDelayed();
}

void QnWorkbenchVideoWallHandler::at_eventManager_controlMessageReceived(const ec2::ApiVideowallControlMessageData& apiMessage)
{
    if (apiMessage.instanceGuid != m_videoWallMode.instanceGuid)
        return;

    QnVideoWallControlMessage message;
    fromApiToResource(apiMessage, message);

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
    // TODO: #GDM #VW what if one message is lost forever? timeout?
    if (!m_videoWallMode.sequenceByPcUuid.contains(controllerUuid) ||
        (sequence - m_videoWallMode.sequenceByPcUuid[controllerUuid] > 1))
    {
#ifdef RECEIVER_DEBUG
        if (!m_videoWallMode.sequenceByPcUuid.contains(controllerUuid))
            qDebug() << "RECEIVER: first message from this controller, waiting for ControlStarted";
        else
            qDebug() << "RECEIVER: current sequence" << m_videoWallMode.sequenceByPcUuid[controllerUuid];
#endif
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
    if (widget->resource()->flags() & Qn::sync)
    {
        if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
        {
            connect(mediaWidget, &QnMediaResourceWidget::motionSelectionChanged, this, &QnWorkbenchVideoWallHandler::at_widget_motionSelectionChanged);
            connect(mediaWidget, &QnMediaResourceWidget::dewarpingParamsChanged, this, &QnWorkbenchVideoWallHandler::at_widget_dewarpingParamsChanged);
        }
    }
}

void QnWorkbenchVideoWallHandler::at_display_widgetAboutToBeRemoved(QnResourceWidget* widget)
{
    disconnect(widget, NULL, this, NULL);
}

void QnWorkbenchVideoWallHandler::at_widget_motionSelectionChanged()
{
    QnMediaResourceWidget* widget = checked_cast<QnMediaResourceWidget *>(sender());
    if (!widget)
        return;

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::MotionSelectionChanged);
    message[uuidKey] = widget->item()->uuid().toString();
    message[valueKey] = QString::fromUtf8(QJson::serialized<QList<QRegion> >(widget->motionSelection()));
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
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbench_currentLayoutAboutToBeChanged()
{
    disconnect(workbench()->currentLayout(), NULL, this, NULL);
    setControlMode(false);
}

void QnWorkbenchVideoWallHandler::at_workbench_currentLayoutChanged()
{
    connect(workbench()->currentLayout(), &QnWorkbenchLayout::dataChanged, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_dataChanged);
    updateMode();
}

void QnWorkbenchVideoWallHandler::at_workbench_itemChanged(Qn::ItemRole role)
{
    if (!m_controlMode.active)
        return;

    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::ItemRoleChanged);
    message[roleKey] = QString::number(role);
    message[uuidKey] = workbench()->item(role) ? workbench()->item(role)->uuid().toString() : QString();
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *item)
{
    connect(item, &QnWorkbenchItem::dataChanged, this, &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemAdded);
    message[uuidKey] = item->uuid().toString();
    message[resourceKey] = item->resource()->getUniqueId();
    message[geometryKey] = QString::fromUtf8(QJson::serialized(item->geometry()));
    message[zoomRectKey] = QString::fromUtf8(QJson::serialized(item->zoomRect()));
    message[rotationKey] = QString::fromUtf8(QJson::serialized(item->rotation()));
    message[checkedButtonsKey] = QString::fromUtf8(QJson::serialized(item->data(Qn::ItemCheckedButtonsRole).toInt()));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem *item)
{
    disconnect(item, &QnWorkbenchItem::dataChanged, this, &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemRemoved);
    message[uuidKey] = item->uuid().toString();
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem)
{
    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::ZoomLinkAdded);
    message[uuidKey] = item->uuid().toString();
    message[zoomUuidKey] = zoomTargetItem->uuid().toString();
    sendMessage(message);
#ifdef SENDER_DEBUG
    qDebug() << "SENDER: zoom Link added" << item->uuid() << zoomTargetItem->uuid();
#endif
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem)
{
    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::ZoomLinkRemoved);
    message[uuidKey] = item->uuid().toString();
    message[zoomUuidKey] = zoomTargetItem->uuid().toString();
    sendMessage(message);
#ifdef SENDER_DEBUG
    qDebug() << "SENDER: zoom Link removed" << item->uuid() << zoomTargetItem->uuid();
#endif
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_dataChanged(int role)
{
    if (role == Qn::VideoWallItemGuidRole || role == Qn::VideoWallResourceRole)
    {
        updateMode();
        return;
    }

    if (!m_controlMode.active)
        return;

    QByteArray json;
    QVariant data = workbench()->currentLayout()->data(role);
    switch (role)
    {
        case Qn::LayoutCellAspectRatioRole:
        case Qn::LayoutCellSpacingRole:
        {
            qreal value = data.toReal();
            QJson::serialize(value, &json);
            break;
        }
        default:
            return; //ignore other fields
    }

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutDataChanged);
    message[roleKey] = QString::number(role);
    message[valueKey] = QString::fromUtf8(json);
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged(Qn::ItemDataRole role)
{
    if (!m_controlMode.active)
        return;

    QnWorkbenchItem* item = static_cast<QnWorkbenchItem *>(sender());
    QByteArray json;
    QVariant data = item->data(role);
    bool cached = false;

    switch (role)
    {
        case Qn::ItemFlagsRole:                 // do not transfer flags, it may result of incorrect pending geometry adjustment
        case Qn::ItemTimeRole:                  // do not transfer static time, it may result of pause
        case Qn::ItemCombinedGeometryRole:      // do not transfer calculated geometry
        case Qn::ItemGeometryDeltaRole:
            return;

        case Qn::ItemPausedRole:
        case Qn::ItemSpeedRole:
        case Qn::ItemPositionRole:
            // do not pause playing if changing layout, else fall-through
            if (display()->isChangingLayout())
                return;

        case Qn::ItemGeometryRole:
        {
            QRect value = data.value<QRect>();
#ifdef SENDER_DEBUG
            qDebug() << "SENDER: Item" << item->uuid() << debugRole(role) << "changed to" << value;
#endif
            QJson::serialize(value, &json);
            break;
        }

        case Qn::ItemZoomRectRole:
        {
            QRectF value = data.value<QRectF>();
#ifdef SENDER_DEBUG
            qDebug() << "SENDER: Item" << item->uuid() << debugRole(role) << "changed to" << value;
#endif
            QJson::serialize(value, &json);
            cached = true;
            break;
        }

        case Qn::ItemRotationRole:
        case Qn::ItemFlipRole:
        {
            qreal value = data.toReal();
#ifdef SENDER_DEBUG
            qDebug() << "SENDER: Item" << item->uuid() << debugRole(role) << "changed to" << value;
#endif
            QJson::serialize(value, &json);
            break;
        }
        case Qn::ItemCheckedButtonsRole:
        {
            int value = data.toInt();
#ifdef SENDER_DEBUG
            qDebug() << "SENDER: Item" << item->uuid() << debugRole(role) << "changed to" << value;
#endif
            QJson::serialize(value, &json);
            break;
        }
        case Qn::ItemFrameDistinctionColorRole:
        {
            QColor value = data.value<QColor>();
#ifdef SENDER_DEBUG
            qDebug() << "SENDER: Item" << item->uuid() << debugRole(role) << "changed to" << value;
#endif
            QJson::serialize(value, &json);
            break;
        }

        case Qn::ItemSliderWindowRole:
        case Qn::ItemSliderSelectionRole:
            QJson::serialize(data.value<QnTimePeriod>(), &json);
            break;

        case Qn::ItemImageEnhancementRole:
            QJson::serialize(data.value<ImageCorrectionParams>(), &json);
            break;

        case Qn::ItemImageDewarpingRole:
            QJson::serialize(data.value<QnItemDewarpingParams>(), &json);
            cached = true;
            break;

        case Qn::ItemHealthMonitoringButtonsRole:
            QJson::serialize(data.value<QnServerResourceWidget::HealthMonitoringButtons>(), &json);
            break;
        default:
#ifdef SENDER_DEBUG
            qDebug() << "SENDER: cannot deserialize" << item->uuid() << debugRole(role) << data;
#endif
            break;
        }

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemDataChanged);
    message[roleKey] = QString::number(role);
    message[uuidKey] = item->uuid().toString();
    message[valueKey] = QString::fromUtf8(json);
    sendMessage(message, cached);
}

void QnWorkbenchVideoWallHandler::at_navigator_positionChanged()
{
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    if (navigator()->positionUsec() == qint64(AV_NOPTS_VALUE))
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::NavigatorPositionChanged);
    message[positionKey] = QString::number(navigator()->positionUsec());
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_navigator_playingChanged()
{
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
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

    QnVideoWallControlMessage message(QnVideoWallControlMessage::NavigatorSpeedChanged);
    message[speedKey] = QString::number(navigator()->speed());
    auto position = navigator()->positionUsec();
    if (position != AV_NOPTS_VALUE)
        message[positionKey] = QString::number(position);
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchStreamSynchronizer_runningChanged()
{
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::SynchronizationChanged);
    message[valueKey] = QString::fromUtf8(QJson::serialized(context()->instance<QnWorkbenchStreamSynchronizer>()->state()));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_controlModeCacheTimer_timeout()
{
    if (m_controlMode.cachedMessages.isEmpty())
        return;

    while (!m_controlMode.cachedMessages.isEmpty())
    {
        QnVideoWallControlMessage message = m_controlMode.cachedMessages.takeLast();
        for (auto iter = m_controlMode.cachedMessages.begin(); iter != m_controlMode.cachedMessages.end();)
        {
            if (
                (iter->operation == message.operation) &&
                (iter->params[roleKey] == message[roleKey]) &&
                (iter->params[uuidKey] == message[uuidKey])
                )
                iter = m_controlMode.cachedMessages.erase(iter);
            else
                iter++;
        }
        sendMessage(message);
    }
}

void QnWorkbenchVideoWallHandler::saveVideowall(const QnVideoWallResourcePtr& videowall, bool saveLayout)
{
    if (saveLayout && QnWorkbenchLayout::instance(videowall))
        saveVideowallAndReviewLayout(videowall);
    else
        qnResourcesChangesManager->saveVideoWall(videowall);
}

void QnWorkbenchVideoWallHandler::saveVideowalls(const QSet<QnVideoWallResourcePtr> &videowalls, bool saveLayout)
{
    foreach(const QnVideoWallResourcePtr &videowall, videowalls)
        saveVideowall(videowall, saveLayout);
}

bool QnWorkbenchVideoWallHandler::saveReviewLayout(const QnLayoutResourcePtr &layoutResource, std::function<void(int, ec2::ErrorCode)> callback)
{
    return saveReviewLayout(QnWorkbenchLayout::instance(layoutResource), callback);
}

bool QnWorkbenchVideoWallHandler::saveReviewLayout(QnWorkbenchLayout *layout, std::function<void(int, ec2::ErrorCode)> callback)
{
    if (!layout)
        return false;

    QSet<QnVideoWallResourcePtr> videowalls;
    for (QnWorkbenchItem* workbenchItem : layout->items())
    {
        QnLayoutItemData data = workbenchItem->data();
        auto indices = getIndices(data);
        if (indices.isEmpty())
            continue;
        QnVideoWallItemIndex firstIdx = indices.first();
        QnVideoWallResourcePtr videowall = firstIdx.videowall();
        QnVideoWallItem item = firstIdx.item();
        QSet<int> screenIndices = item.screenSnaps.screens();
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

    // TODO: #GDM #VW refactor saving to simplier logic
    // TODO: #GDM #VW sometimes saving is not required
    // TODO: #GDM SafeMode
    for (const QnVideoWallResourcePtr &videowall : videowalls)
    {
        ec2::ApiVideowallData apiVideowall;
        fromResourceToApi(videowall, apiVideowall);
        connection2()->getVideowallManager(Qn::kSystemAccess)->save(apiVideowall, this,
            [this, callback](int reqID, ec2::ErrorCode errorCode)
        {
            callback(reqID, errorCode);
        });
    }

    return !videowalls.isEmpty();
}

void QnWorkbenchVideoWallHandler::setItemOnline(const QnUuid &instanceGuid, bool online)
{
    NX_ASSERT(!instanceGuid.isNull());

    QnVideoWallItemIndex index = resourcePool()->getVideoWallItemByUuid(instanceGuid);
    if (index.isNull())
        return;

    QnVideoWallItem item = index.item();
    item.runtimeStatus.online = online;
    index.videowall()->items()->updateItem(item);

    if (item.uuid == workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).value<QnUuid>())
        updateMode();
}

void QnWorkbenchVideoWallHandler::setItemControlledBy(const QnUuid &layoutId, const QnUuid &controllerId, bool on)
{
    bool needUpdate = false;
    auto layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;

    QnUuid currentId = layout->getId();
    foreach(const QnVideoWallResourcePtr &videoWall, resourcePool()->getResources<QnVideoWallResource>())
    {
        foreach(const QnVideoWallItem &item, videoWall->items()->getItems())
        {
            if (!on && item.runtimeStatus.controlledBy == controllerId)
            {
                /* Check layouts that were previously controlled by this client. */
                needUpdate |= (item.layout == currentId && !item.layout.isNull());

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
    if (controllerId != commonModule()->moduleGUID() && needUpdate)
        updateMode();
}


void QnWorkbenchVideoWallHandler::updateMainWindowGeometry(const QnScreenSnaps &screenSnaps)
{
    QList<QRect> screens;
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i)
        screens << desktop->screenGeometry(i);
    QRect targetGeometry = screenSnaps.geometry(screens);
    mainWindow()->setGeometry(targetGeometry);
}

void QnWorkbenchVideoWallHandler::updateControlLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action)
{
    QN_UNUSED(videowall);
    if (action == ItemAction::Changed)
    {

        // index to place updated layout
        int layoutIndex = -1;
        bool wasCurrent = false;

        // check if layout was changed or detached
        for (int i = 0; i < workbench()->layouts().size(); ++i)
        {
            QnWorkbenchLayout *layout = workbench()->layout(i);

            if (layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>() != item.uuid)
                continue;

            layout->notifyTitleChanged();   //in case of 'online' flag changed

            if (layout->resource() && item.layout == layout->resource()->getId())
                return; //everything is correct, no changes required

            wasCurrent = workbench()->currentLayout() == layout;
            layoutIndex = i;
            layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(QnUuid()));
            workbench()->removeLayout(layout);
        }

        if (item.layout.isNull() || layoutIndex < 0)
            return;

        // add new layout if needed
        {
            QnLayoutResourcePtr layoutResource = resourcePool()->getResourceById<QnLayoutResource>(item.layout);
            if (!layoutResource)
                return;

            QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(layoutResource);
            if (!layout)
                layout = qnWorkbenchLayoutsFactory->create(layoutResource, workbench());

            if (workbench()->layoutIndex(layout) < 0)
                workbench()->insertLayout(layout, layoutIndex);
            else
                workbench()->moveLayout(layout, layoutIndex);

            layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(item.uuid));
            layout->notifyTitleChanged();
            if (wasCurrent)
                workbench()->setCurrentLayoutIndex(layoutIndex);
        }
    }
    else if (action == ItemAction::Removed)
    {
        for (int i = 0; i < workbench()->layouts().size(); ++i)
        {
            QnWorkbenchLayout *layout = workbench()->layout(i);

            if (layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>() != item.uuid)
                continue;
            layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(QnUuid()));
            layout->notifyTitleChanged();
        }
    }
}

void QnWorkbenchVideoWallHandler::updateReviewLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action)
{
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videowall);
    if (!layout)
        return;

    auto findCurrentWorkbenchItem = [layout, &item]() -> QnWorkbenchItem*
    {
        foreach(QnWorkbenchItem *workbenchItem, layout->items())
        {
            QnLayoutItemData data = workbenchItem->data();
            auto indices = getIndices(data);
            if (indices.isEmpty())
                continue;

            // checking existing widgets containing target item
            foreach(const QnVideoWallItemIndex &index, indices)
            {
                if (index.uuid() != item.uuid)
                    continue;
                return workbenchItem;
            }
        }
        return nullptr;
    };

    auto findTargetWorkbenchItem = [layout, &item]() -> QnWorkbenchItem*
    {
        QnWorkbenchItem *currentItem = nullptr;
        foreach(QnWorkbenchItem *workbenchItem, layout->items())
        {
            QnLayoutItemData data = workbenchItem->data();
            auto indices = getIndices(data);
            if (indices.isEmpty())
                continue;

            // checking existing widgets with same screen sets
            // take any other item on this widget
            const auto other = std::find_if(indices.cbegin(), indices.cend(),
                [&item](const QnVideoWallItemIndex &idx) { return idx.uuid() != item.uuid; });
            if (other != indices.cend()
                && (other->item().pcUuid == item.pcUuid)
                && (other->item().screenSnaps.screens() == item.screenSnaps.screens()))
            {
                return workbenchItem;
            }

            // our item is the only item on the widget, we can modify it as we want
            if (indices.size() == 1 && indices.first().item().uuid == item.uuid)
                currentItem = workbenchItem;

            //continue search to more sufficient widgets
        }
        return currentItem;
    };

    auto removeFromWorkbenchItem = [layout, &videowall, &item](QnWorkbenchItem* workbenchItem)
    {
        if (!workbenchItem)
            return;

        auto data = workbenchItem->data();
        auto indices = getIndices(data);
        indices.removeAll(QnVideoWallItemIndex(videowall, item.uuid));
        if (indices.isEmpty())
            layout->removeItem(workbenchItem);
        else
            setIndices(data, indices);
    };

    auto addToWorkbenchItem = [this, layout, &videowall, &item](QnWorkbenchItem* workbenchItem)
    {
        if (workbenchItem)
        {
            auto data = workbenchItem->data();
            auto indices = getIndices(data);
            indices << QnVideoWallItemIndex(videowall, item.uuid);
            setIndices(data, indices);
        }
        else
        {
            addItemToLayout(layout->resource(), QnVideoWallItemIndexList() << QnVideoWallItemIndex(videowall, item.uuid));
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

bool QnWorkbenchVideoWallHandler::checkLocalFiles(const QnVideoWallItemIndex& index,
    const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return true;

    return messages::Videowall::checkLocalFiles(mainWindow(), index,
        layout->layoutResources().toList());
}

bool QnWorkbenchVideoWallHandler::validateLicenses(const QString &detail) const
{
    // TODO: #GDM add "Licenses" button
    if (!m_licensesHelper->isValid())
    {
        QnMessageBox::warning(mainWindow(), tr("More Video Wall licenses required"), detail);
        return false;
    }
    return true;
}

QnUuid QnWorkbenchVideoWallHandler::getLayoutController(const QnUuid &layoutId)
{
    if (layoutId.isNull())
        return QnUuid();

    foreach(const QnPeerRuntimeInfo &info, runtimeInfoManager()->items()->getItems())
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
    QnWorkbenchLayoutsChangeValidator validator(context());
    return validator.confirmChangeVideoWallLayout(layout, resources);
}

void QnWorkbenchVideoWallHandler::saveVideowallAndReviewLayout(const QnVideoWallResourcePtr& videowall, const QnLayoutResourcePtr &layout)
{
    QnWorkbenchLayout* workbenchLayout = layout
        ? QnWorkbenchLayout::instance(layout)
        : QnWorkbenchLayout::instance(videowall);

    if (const auto reviewLayout = workbenchLayout->resource())
    {
        auto callback =
            [this, id = reviewLayout->getId()](int /*reqId*/, ec2::ErrorCode errorCode)
            {
                snapshotManager()->markBeingSaved(id, false);
                if (errorCode != ec2::ErrorCode::ok)
                    return;
                snapshotManager()->markChanged(id, false);
            };
        if (saveReviewLayout(reviewLayout, callback))
            return;
    }

    qnResourcesChangesManager->saveVideoWall(videowall);
}

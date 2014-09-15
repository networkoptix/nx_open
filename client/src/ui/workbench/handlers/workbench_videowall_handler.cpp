#include "workbench_videowall_handler.h"

#include <QtCore/QProcess>
#include <QtCore/QRegExp>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <client/client_message_processor.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/resource_type.h>
#include <core/resource/resource_name.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource_management/resource_pool.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <platform/platform_abstraction.h>

#include <recording/time_period.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/dialogs/layout_name_dialog.h> //TODO: #GDM #VW refactor
#include <ui/dialogs/attach_to_videowall_dialog.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/dialogs/videowall_settings_dialog.h>
#include <ui/dialogs/checkable_message_box.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_auto_starter.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>

#include <utils/color_space/image_correction.h>
#include <utils/common/checked_cast.h>
#include <utils/common/collection.h>
#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>
#include <utils/common/string.h>
#include <utils/license_usage_helper.h>

#include <utils/common/app_info.h>

//#define SENDER_DEBUG
//#define RECEIVER_DEBUG

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

#if defined(SENDER_DEBUG) || defined(RECEIVER_DEBUG)
    static QByteArray debugRole(int role) {
        switch (role) {
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

    void addItemToLayout(const QnLayoutResourcePtr &layout, const QnVideoWallItemIndexList& indices) {
        if (indices.isEmpty())
            return;

        QnVideoWallItemIndex firstIdx = indices.first();
        if (!firstIdx.isValid())
            return;

        QList<int> screens = firstIdx.item().screenSnaps.screens().toList();
        if (screens.isEmpty())
            return;

        QnVideoWallPcData pc = firstIdx.videowall()->pcs()->getItem(firstIdx.item().pcUuid);
        if (pc.screens.size() < screens.first())
            return;

        QnLayoutItemData itemData;
        itemData.uuid = QUuid::createUuid();
        itemData.combinedGeometry = pc.screens[screens.first()].layoutGeometry;
        if (itemData.combinedGeometry.isValid())
            itemData.flags = Qn::Pinned;
        else
            itemData.flags = Qn::PendingGeometryAdjustment;
        itemData.resource.id = firstIdx.videowall()->getId();
        itemData.resource.path = firstIdx.videowall()->getUniqueId();
        itemData.dataByRole[Qn::VideoWallItemIndicesRole] = qVariantFromValue<QnVideoWallItemIndexList>(indices);
        layout->addItem(itemData);
    }

    struct ScreenWidgetKey{
        QUuid pcUuid;
        QSet<int> screens;

        ScreenWidgetKey(const QUuid &pcUuid, const QSet<int> screens):
            pcUuid(pcUuid), screens(screens){}

        friend bool operator==(const ScreenWidgetKey &l, const ScreenWidgetKey &r) {
            return l.pcUuid == r.pcUuid && l.screens == r.screens;
        }

        friend bool operator<(const ScreenWidgetKey &l, const ScreenWidgetKey &r) {
            if (l.pcUuid != r.pcUuid || (l.screens.isEmpty() && r.screens.isEmpty()))
                return l.pcUuid < r.pcUuid;
            auto lmin = std::min_element(l.screens.constBegin(), l.screens.constEnd());
            auto rmin = std::min_element(r.screens.constBegin(), r.screens.constEnd());
            return (*lmin) < (*rmin);
        }
    };

    const int identifyTimeout  = 5000;
    const int identifyFontSize = 100;

    const int cacheMessagesTimeoutMs = 500;

    const qreal defaultReviewAR = 1920.0 / 1080.0;

    /** Minimal amount of licenses to allow videowall creating. */
    const int videowallStarterPackAmount = 5;
}

class QnVideowallAutoStarter: public QnWorkbenchAutoStarter {
public:
    QnVideowallAutoStarter(const QUuid &videowallUuid, QObject *parent = NULL): 
        QnWorkbenchAutoStarter(parent),
        m_videoWallUuid(videowallUuid) 
    {}

protected:
    virtual int settingsKey() const override { return -1; }

    virtual QString autoStartPath() const override {
        QStringList arguments;
        arguments << lit("--videowall");
        arguments << m_videoWallUuid.toString();
        QUrl url = QnAppServerConnectionFactory::url();
        url.setUserName(QString());
        url.setPassword(QString());
        arguments << lit("--auth");
        arguments << QString::fromUtf8(url.toEncoded());

        QFileInfo clientFile = QFileInfo(qApp->applicationFilePath());
        QString result = toRegistryFormat(clientFile.canonicalFilePath()) + L' ' + arguments.join(L' ');
        return result;
    }

    virtual QString autoStartKey() const override { return qApp->applicationName() + L' ' + m_videoWallUuid.toString(); }
private:
    QUuid m_videoWallUuid;
};

class QnVideowallReviewLayoutResource: public QnLayoutResource {
public:
    QnVideowallReviewLayoutResource(const QnVideoWallResourcePtr &videowall):
        QnLayoutResource(qnResTypePool)
    {
        setId(QUuid::createUuid());
        addFlags(Qn::local);
        setName(videowall->getName());
        setCellSpacing(0.1, 0.1);
        setCellAspectRatio(defaultReviewAR);
        setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission));
        setData(Qn::VideoWallResourceRole, qVariantFromValue(videowall));

        connect(videowall.data(), &QnResource::nameChanged, this, [this](const QnResourcePtr &resource){setName(resource->getName());});
    }
};

QnWorkbenchVideoWallHandler::QnWorkbenchVideoWallHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_licensesHelper(new QnVideoWallLicenseUsageHelper())
{
    m_videoWallMode.active = qnSettings->isVideoWallMode();
    m_videoWallMode.opening = false;
    m_videoWallMode.ready = false;
    m_controlMode.active = false;
    m_controlMode.sequence = 0;
    m_controlMode.cacheTimer = new QTimer(this);
    m_controlMode.cacheTimer->setInterval(cacheMessagesTimeoutMs);
    connect(m_controlMode.cacheTimer, &QTimer::timeout, this, &QnWorkbenchVideoWallHandler::at_controlModeCacheTimer_timeout);

    QUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull()) {
        pcUuid = QUuid::createUuid();
        qnSettings->setPcUuid(pcUuid);
    }
    m_controlMode.pcUuid = pcUuid.toString();

    /* Common connections */
    connect(resourcePool(), &QnResourcePool::resourceAdded,     this,   &QnWorkbenchVideoWallHandler::at_resPool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,   this,   &QnWorkbenchVideoWallHandler::at_resPool_resourceRemoved);
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resPool_resourceAdded(resource);

    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, [this](const QnPeerRuntimeInfo &info) {
        if (info.data.peer.peerType != Qn::PT_VideowallClient)
            return;
        setItemOnline(info.data.videoWallInstanceGuid, true);
    });
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoRemoved, this, [this](const QnPeerRuntimeInfo &info) {
        if (info.data.peer.peerType != Qn::PT_VideowallClient)
            return;
        setItemOnline(info.data.videoWallInstanceGuid, false);
    });
    foreach (const QnPeerRuntimeInfo &info, QnRuntimeInfoManager::instance()->items()->getItems()) {
        if (info.data.peer.peerType != Qn::PT_VideowallClient)
            continue;
        setItemOnline(info.data.videoWallInstanceGuid, true);
    }


    if (m_videoWallMode.active) {
        /* Videowall reaction actions */

        connect(action(Qn::DelayedOpenVideoWallItemAction), &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_delayedOpenVideoWallItemAction_triggered);

        QnCommonMessageProcessor* clientMessageProcessor = QnClientMessageProcessor::instance();
        connect(clientMessageProcessor,   &QnCommonMessageProcessor::videowallControlMessageReceived,
                this,                     &QnWorkbenchVideoWallHandler::at_eventManager_controlMessageReceived);

    } else {

        /* Control videowall actions */

        connect(action(Qn::NewVideoWallAction),             &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered);
        connect(action(Qn::AttachToVideoWallAction),        &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered);
        connect(action(Qn::DetachFromVideoWallAction),      &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered);
        connect(action(Qn::ResetVideoWallLayoutAction),     &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_resetVideoWallLayoutAction_triggered);
        connect(action(Qn::DeleteVideoWallItemAction),      &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_deleteVideoWallItemAction_triggered);
        connect(action(Qn::StartVideoWallAction),           &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_startVideoWallAction_triggered);
        connect(action(Qn::StopVideoWallAction),            &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_stopVideoWallAction_triggered);
        connect(action(Qn::RenameAction),                   &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_renameAction_triggered);
        connect(action(Qn::IdentifyVideoWallAction),        &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_identifyVideoWallAction_triggered);
        connect(action(Qn::StartVideoWallControlAction),    &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered);
        connect(action(Qn::OpenVideoWallsReviewAction),     &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_openVideoWallsReviewAction_triggered);
        connect(action(Qn::SaveCurrentVideoWallReviewAction),&QAction::triggered,       this,   &QnWorkbenchVideoWallHandler::at_saveCurrentVideoWallReviewAction_triggered);
        connect(action(Qn::SaveVideoWallReviewAction),      &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered);
        connect(action(Qn::DropOnVideoWallItemAction),      &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_dropOnVideoWallItemAction_triggered);
        connect(action(Qn::PushMyScreenToVideowallAction),  &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_pushMyScreenToVideowallAction_triggered);
        connect(action(Qn::VideowallSettingsAction),        &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_videowallSettingsAction_triggered);
        connect(action(Qn::SaveVideowallMatrixAction),      &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_saveVideowallMatrixAction_triggered);
        connect(action(Qn::LoadVideowallMatrixAction),      &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_loadVideowallMatrixAction_triggered);
        connect(action(Qn::DeleteVideowallMatrixAction),    &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_deleteVideowallMatrixAction_triggered);
        

        connect(display(),     &QnWorkbenchDisplay::widgetAdded,                        this,   &QnWorkbenchVideoWallHandler::at_display_widgetAdded);
        connect(display(),     &QnWorkbenchDisplay::widgetAboutToBeRemoved,             this,   &QnWorkbenchVideoWallHandler::at_display_widgetAboutToBeRemoved);

        connect(workbench(),   &QnWorkbench::currentLayoutAboutToBeChanged,             this,   &QnWorkbenchVideoWallHandler::at_workbench_currentLayoutAboutToBeChanged);
        connect(workbench(),   &QnWorkbench::currentLayoutChanged,                      this,   &QnWorkbenchVideoWallHandler::at_workbench_currentLayoutChanged);

        connect(navigator(),   &QnWorkbenchNavigator::positionChanged,                  this,   &QnWorkbenchVideoWallHandler::at_navigator_positionChanged);
        connect(navigator(),   &QnWorkbenchNavigator::speedChanged,                     this,   &QnWorkbenchVideoWallHandler::at_navigator_speedChanged);

        connect(context()->instance<QnWorkbenchStreamSynchronizer>(),   &QnWorkbenchStreamSynchronizer::runningChanged,
                this,                                                   &QnWorkbenchVideoWallHandler::at_workbenchStreamSynchronizer_runningChanged);

        foreach(QnResourceWidget *widget, display()->widgets())
            at_display_widgetAdded(widget);
    }
}

QnWorkbenchVideoWallHandler::~QnWorkbenchVideoWallHandler() {

}

ec2::AbstractECConnectionPtr QnWorkbenchVideoWallHandler::connection2() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchVideoWallHandler::resetLayout(const QnVideoWallItemIndexList &items, const QnLayoutResourcePtr &layout) {
    if (items.isEmpty())
        return;

    layout->setCellSpacing(QSizeF(0.0, 0.0));
    layout->setUserCanEdit(true);

    auto reset = [this](const QnVideoWallItemIndexList &items, const QnLayoutResourcePtr &layout) {
        updateItemsLayout(items, layout->getId());
    };

    if (snapshotManager()->isLocal(layout) || snapshotManager()->isModified(layout)) {
        QnLayoutResourceList unsavedLayouts;
        unsavedLayouts << layout;
        QnWorkbenchLayoutReplyProcessor *processor = new QnWorkbenchLayoutReplyProcessor(snapshotManager(), unsavedLayouts);
        connect(processor, &QnWorkbenchLayoutReplyProcessor::finished, this,
            [this, items, layout, reset](int status, const QnResourceList &resources, int handle) {
            Q_UNUSED(resources)
            Q_UNUSED(handle)
            if (status != 0)
                QMessageBox::warning(mainWindow(), tr("Error"), tr("Unexpected error has occurred. Changes cannot be saved."));
            else
                reset(items, layout);
        });
        snapshotManager()->save(unsavedLayouts, processor);
    } else {
        reset(items, layout);
    }
}

void QnWorkbenchVideoWallHandler::swapLayouts(const QnVideoWallItemIndex firstIndex, const QnLayoutResourcePtr &firstLayout, const QnVideoWallItemIndex &secondIndex, const QnLayoutResourcePtr &secondLayout) {
    if (!firstIndex.isValid() || !secondIndex.isValid())
        return;

    QnLayoutResourceList unsavedLayouts;
    if (snapshotManager()->isLocal(firstLayout) || snapshotManager()->isModified(firstLayout))
        unsavedLayouts << firstLayout;

    if (snapshotManager()->isLocal(secondLayout) || snapshotManager()->isModified(secondLayout))
        unsavedLayouts << secondLayout;

    auto swap = [this](const QnVideoWallItemIndex firstIndex, const QnLayoutResourcePtr &firstLayout, const QnVideoWallItemIndex &secondIndex, const QnLayoutResourcePtr &secondLayout) {
        QnVideoWallItem firstItem = firstIndex.item();
        firstItem.layout = firstLayout->getId();
        firstIndex.videowall()->items()->updateItem(firstIndex.uuid(), firstItem);

        QnVideoWallItem secondItem = secondIndex.item();
        secondItem.layout = secondLayout->getId();
        secondIndex.videowall()->items()->updateItem(secondIndex.uuid(), secondItem);

        saveVideowalls(QSet<QnVideoWallResourcePtr>() << firstIndex.videowall() << secondIndex.videowall());
    };

    if (!unsavedLayouts.isEmpty()) {
        QnWorkbenchLayoutReplyProcessor *processor = new QnWorkbenchLayoutReplyProcessor(snapshotManager(), unsavedLayouts);
        connect(processor, &QnWorkbenchLayoutReplyProcessor::finished, this, 
            [this, firstIndex, firstLayout, secondIndex, secondLayout, swap](int status, const QnResourceList &resources, int handle) {
            Q_UNUSED(resources)
            Q_UNUSED(handle)
            if (status != 0)
                QMessageBox::warning(mainWindow(), tr("Error"), tr("Unexpected error has occurred. Changes cannot be saved."));
            else
                swap(firstIndex, firstLayout, secondIndex, secondLayout);
        });
        snapshotManager()->save(unsavedLayouts, processor);
    } else {
        swap(firstIndex, firstLayout, secondIndex, secondLayout);
    }
}

void QnWorkbenchVideoWallHandler::updateItemsLayout(const QnVideoWallItemIndexList &items, const QUuid &layoutId) {
    QSet<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &index, items) {
        if (!index.isValid())
            continue;

        QnVideoWallItem existingItem = index.item();
        if (existingItem.layout == layoutId)
            continue;

        existingItem.layout = layoutId;
        index.videowall()->items()->updateItem(index.uuid(), existingItem);
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls);
}

bool QnWorkbenchVideoWallHandler::canStartVideowall(const QnVideoWallResourcePtr &videowall) {
    QUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull()) {
        qWarning() << "Warning: pc UUID is null, cannot start Video Wall on this pc";
        return false;
    }

    foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
        if (item.pcUuid != pcUuid || item.online)
            continue;
        return true;
    }
    return false;
}

void QnWorkbenchVideoWallHandler::startVideowallAndExit(const QnVideoWallResourcePtr &videoWall) {
    if (!canStartVideowall(videoWall)) {
        QMessageBox::warning(mainWindow(),
            tr("Error"),
            tr("There are no offline videowall items attached to this pc.")); //TODO: #VW #TR
        return;
    }

    QMessageBox::StandardButton button = 
        QMessageBox::question(
            mainWindow(),
            tr("Switch to Video Wall Mode..."),
            tr("Video Wall will be started now. Do you want to close this %1 Client instance?")
                .arg(QnAppInfo::productNameLong()), //TODO: #VW #TR
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes
            );

    if (button == QMessageBox::Cancel)
        return;

    if (button == QMessageBox::Yes) {
        closeInstanceDelayed();
    }
    
    QUuid pcUuid = qnSettings->pcUuid();
    foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
        if (item.pcUuid != pcUuid || item.online)
            continue;

        QStringList arguments;
        arguments << lit("--videowall");
        arguments << videoWall->getId().toString();
        arguments << lit("--videowall-instance");
        arguments << item.uuid.toString();
        openNewWindow(arguments);
    }
}

void QnWorkbenchVideoWallHandler::openNewWindow(const QStringList &args) {
    QStringList arguments = args;

    QUrl url = QnAppServerConnectionFactory::url();
    url.setUserName(QString());
    url.setPassword(QString());

    arguments << lit("--auth");
    arguments << QString::fromUtf8(url.toEncoded());

#ifdef SENDER_DEBUG
    qDebug() << "arguments" << arguments;
#endif
    QProcess::startDetached(qApp->applicationFilePath(), arguments);
}

void QnWorkbenchVideoWallHandler::openVideoWallItem(const QnVideoWallResourcePtr &videoWall) {
    if (!videoWall) {
        qWarning() << "Warning: videowall not exists anymore, cannot open videowall item";
        closeInstanceDelayed();
        return;
    }

    workbench()->clear();

    QnVideoWallItem item = videoWall->items()->getItem(m_videoWallMode.instanceGuid);

    QnLayoutResourcePtr layout = qnResPool->getResourceById(item.layout).dynamicCast<QnLayoutResource>();
    if (layout)
        menu()->trigger(Qn::OpenSingleLayoutAction, layout);

    updateMainWindowGeometry(item.screenSnaps);
}

void QnWorkbenchVideoWallHandler::closeInstanceDelayed() {
    menu()->trigger(Qn::ExitActionDelayed);
}

void QnWorkbenchVideoWallHandler::sendMessage(QnVideoWallControlMessage message, bool cached) {
    Q_ASSERT(m_controlMode.active);

    if (cached) {
        m_controlMode.cachedMessages << message;
        return;
    }

    message[sequenceKey] = QString::number(m_controlMode.sequence++);
    message[pcUuidKey] = m_controlMode.pcUuid;
#ifdef SENDER_DEBUG
    qDebug() << "SENDER: sending message" << message;
#endif
    foreach (QnVideoWallItemIndex index, targetList()) {
        message.videoWallGuid = index.videowall()->getId();
        message.instanceGuid = index.uuid();
        connection2()->getVideowallManager()->sendControlMessage(message, this, []{});
    }
}

void QnWorkbenchVideoWallHandler::handleMessage(const QnVideoWallControlMessage &message, const QUuid &controllerUuid, qint64 sequence) {
#ifdef RECEIVER_DEBUG
    qDebug() << "RECEIVER: handling message" << message;
#endif

    if (sequence >= 0)
        m_videoWallMode.sequenceByPcUuid[controllerUuid] = sequence;

    switch (static_cast<QnVideoWallControlMessage::Operation>(message.operation)) {
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
        while (i != stored.end()) {
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
        QUuid guid(message[uuidKey]);
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
        switch (role) {
        case Qn::LayoutCellSpacingRole:
        {
            QSizeF data = QJson::deserialized<QSizeF>(value);
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

        QUuid uuid = QUuid(message[uuidKey]);
        if (workbench()->currentLayout()->item(uuid))
            return;

        QString resourceUid = message[resourceKey];
        QRect geometry = QJson::deserialized<QRect>(message[geometryKey].toUtf8());
        QRectF zoomRect = QJson::deserialized<QRectF>(message[zoomRectKey].toUtf8());
        qreal rotation = QJson::deserialized<qreal>(message[rotationKey].toUtf8());
        int checkedButtons = QJson::deserialized<int>(message[checkedButtonsKey].toUtf8());
        QnWorkbenchItem* item = new QnWorkbenchItem(resourceUid, uuid, workbench()->currentLayout());
        item->setGeometry(geometry);
        item->setZoomRect(zoomRect);
        item->setRotation(rotation);
        item->setFlag(Qn::PendingGeometryAdjustment);
        item->setData(Qn::ItemCheckedButtonsRole, checkedButtons);
        workbench()->currentLayout()->addItem(item);

#ifdef RECEIVER_DEBUG
        qDebug() << "RECEIVER: Item"  << uuid << "added to" << geometry;
#endif
        break;
    }
    case QnVideoWallControlMessage::LayoutItemRemoved:
    {
        QUuid uuid = QUuid(message[uuidKey]);
        if (QnWorkbenchItem* item = workbench()->currentLayout()->item(uuid))
            workbench()->currentLayout()->removeItem(item);
        break;
    }
    case QnVideoWallControlMessage::LayoutItemDataChanged:
    {
        QByteArray value = message[valueKey].toUtf8();
        QUuid uuid = QUuid(message[uuidKey]);
        if (!workbench()->currentLayout()->item(uuid))
            return;

        int role = message[roleKey].toInt();

        switch (role) {
        case Qn::ItemGeometryRole:
        {
            QRect data = QJson::deserialized<QRect>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
#ifdef RECEIVER_DEBUG
            qDebug() << "RECEIVER: Item"  << uuid <<  "geometry changed to" << data;
#endif
            break;
        }
        case Qn::ItemGeometryDeltaRole:
        case Qn::ItemCombinedGeometryRole:
        case Qn::ItemZoomRectRole:
        {
            QRectF data = QJson::deserialized<QRectF>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
#ifdef RECEIVER_DEBUG
            qDebug() << "RECEIVER: Item" << uuid <<  debugRole(role) << "changed to" << data;
#endif
            break;
        }
        case Qn::ItemPositionRole:
        {
            QPointF data = QJson::deserialized<QPointF>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemRotationRole:
        case Qn::ItemSpeedRole:
        {
            qreal data;
            QJson::deserialize(value, &data);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemFlipRole:
        case Qn::ItemPausedRole:
        {
            bool data;
            QJson::deserialize(value, &data);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemCheckedButtonsRole:
        {
            int data;
            QJson::deserialize(value, &data);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemFrameDistinctionColorRole:
        {
            QColor data = QJson::deserialized<QColor>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemSliderWindowRole:
        case Qn::ItemSliderSelectionRole:
        {
            QnTimePeriod data = QJson::deserialized<QnTimePeriod>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemImageEnhancementRole:
        {
            ImageCorrectionParams data = QJson::deserialized<ImageCorrectionParams>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemImageDewarpingRole:
        {
            QnItemDewarpingParams data = QJson::deserialized<QnItemDewarpingParams>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }
        case Qn::ItemHealthMonitoringButtonsRole:
        {
            QnServerResourceWidget::HealthMonitoringButtons data = QJson::deserialized<QnServerResourceWidget::HealthMonitoringButtons>(value);
            workbench()->currentLayout()->item(uuid)->setData(role, data);
            break;
        }

        default:
            break;
        }

        break;
    }
    case QnVideoWallControlMessage::ZoomLinkAdded:
    {
        QnWorkbenchItem* item = workbench()->currentLayout()->item(QUuid(message[uuidKey]));
        QnWorkbenchItem* zoomTargetItem = workbench()->currentLayout()->item(QUuid(message[zoomUuidKey]));
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
        QnWorkbenchItem* item = workbench()->currentLayout()->item(QUuid(message[uuidKey]));
        QnWorkbenchItem* zoomTargetItem = workbench()->currentLayout()->item(QUuid(message[zoomUuidKey]));
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
    case QnVideoWallControlMessage::NavigatorSpeedChanged:
    {
        navigator()->setSpeed(message[speedKey].toDouble());
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
        QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(QUuid(message[uuidKey])));
        if (widget)
            widget->setMotionSelection(regions);
        break;
    }
    case QnVideoWallControlMessage::MediaDewarpingParamsChanged:
    {
        QByteArray value = message[valueKey].toUtf8();
        QnMediaDewarpingParams params = QJson::deserialized<QnMediaDewarpingParams>(value);
        QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(QUuid(message[uuidKey])));
        if (widget)
            widget->setDewarpingParams(params);
        break;
    }
    case QnVideoWallControlMessage::Identify:
    {
        QnVideoWallResourcePtr videoWall = qnResPool->getResourceById(m_videoWallMode.guid).dynamicCast<QnVideoWallResource>();
        if (!videoWall)
            return;

        QnVideoWallItem data = videoWall->items()->getItem(m_videoWallMode.instanceGuid);
        if (!data.name.isEmpty())
            QnGraphicsMessageBox::information(data.name, identifyTimeout, identifyFontSize);
        break;
    }
    default:
        break;
    }
}

void QnWorkbenchVideoWallHandler::storeMessage(const QnVideoWallControlMessage &message, const QUuid &controllerUuid,  qint64 sequence) {
    m_videoWallMode.storedMessages[controllerUuid][sequence] = message;
#ifdef RECEIVER_DEBUG
    qDebug() << "RECEIVER:" << "store message" << message;
#endif
}

void QnWorkbenchVideoWallHandler::restoreMessages(const QUuid &controllerUuid, qint64 sequence) {
    StoredMessagesHash &stored = m_videoWallMode.storedMessages[controllerUuid];
    while (stored.contains(sequence + 1)) {
        QnVideoWallControlMessage message = stored.take(++sequence);
#ifdef RECEIVER_DEBUG
        qDebug() << "RECEIVER:" << "restored message" << message;
#endif
        handleMessage(message, controllerUuid, sequence);
    }
}

void QnWorkbenchVideoWallHandler::setControlMode(bool active) {
    if (active) {
        QnLicenseListHelper licenseList(qnLicensePool->getLicenses());
        if (licenseList.totalLicenseByType(Qn::LC_VideoWall) < videowallStarterPackAmount) {
            QMessageBox::warning(mainWindow(),
                tr("More licenses required"),
                tr("To enable the feature please activate Video Wall starter license"));
            return;
        }

        QnVideoWallLicenseUsageProposer proposer(m_licensesHelper.data(), 1);
        if (!validateLicenses(tr("Could not start Video Wall control session."))) {
            workbench()->currentLayout()->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(QUuid()));
            workbench()->currentLayout()->notifyTitleChanged();
            return;
        }
    }

    if (m_controlMode.active == active)
        return;

    m_controlMode.cachedMessages.clear();

    QnWorkbenchLayout* layout = workbench()->currentLayout();
    if (active) {
        connect(workbench(),    &QnWorkbench::itemChanged,              this,   &QnWorkbenchVideoWallHandler::at_workbench_itemChanged);
        connect(layout,         &QnWorkbenchLayout::itemAdded,          this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode);
        connect(layout,         &QnWorkbenchLayout::itemRemoved,        this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode);
        connect(layout,         &QnWorkbenchLayout::zoomLinkAdded,      this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded);
        connect(layout,         &QnWorkbenchLayout::zoomLinkRemoved,    this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved);

        foreach (QnWorkbenchItem* item, layout->items()) {
             connect(item,      &QnWorkbenchItem::dataChanged,          this,   &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);
        }

        m_controlMode.active = active;
        m_controlMode.cacheTimer->start();
        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStarted));  //TODO: #GDM #VW start control when item goes online

        QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
        localInfo.data.videoWallControlSessions++;
        QnRuntimeInfoManager::instance()->items()->updateItem(localInfo.uuid, localInfo);
    } else {
        disconnect(workbench(),    &QnWorkbench::itemChanged,           this,   &QnWorkbenchVideoWallHandler::at_workbench_itemChanged);
        disconnect(layout,         &QnWorkbenchLayout::itemAdded,       this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode);
        disconnect(layout,         &QnWorkbenchLayout::itemRemoved,     this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode);
        disconnect(layout,         &QnWorkbenchLayout::zoomLinkAdded,   this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded);
        disconnect(layout,         &QnWorkbenchLayout::zoomLinkRemoved, this,   &QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved);

        foreach (QnWorkbenchItem* item, layout->items()) {
             disconnect(item,      &QnWorkbenchItem::dataChanged,       this,   &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);
        }

        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStopped));
        m_controlMode.active = active;
        m_controlMode.cacheTimer->stop();

        QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
        localInfo.data.videoWallControlSessions--;
        QnRuntimeInfoManager::instance()->items()->updateItem(localInfo.uuid, localInfo);
    }
}

void QnWorkbenchVideoWallHandler::updateMode() {
    QnWorkbenchLayout* layout = workbench()->currentLayout();

    QUuid itemUuid = layout->data(Qn::VideoWallItemGuidRole).value<QUuid>();
    bool control = false;
    if (!itemUuid.isNull()) {
        QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(itemUuid);
        if (!index.isNull() && index.item().online)
            control = true;
    }
    setControlMode(control);
}

void QnWorkbenchVideoWallHandler::submitDelayedItemOpen() {

    // not logged in yet
    if (!m_videoWallMode.ready)
        return;

    // already opened or not submitted yet
    if (!m_videoWallMode.opening)
        return;

    m_videoWallMode.opening = false;

    QnVideoWallResourcePtr videoWall = qnResPool->getResourceById(m_videoWallMode.guid).dynamicCast<QnVideoWallResource>();
    if (!videoWall || videoWall->items()->getItems().isEmpty()) {
        if (!videoWall)
            qWarning() << "Warning: videowall not exists, cannot start videowall on this pc";
        else
            qWarning() << "Warning: videowall is empty, cannot start videowall on this pc";
        closeInstanceDelayed();
        return;
    }

    QUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull()) {
        qWarning() << "Warning: pc UUID is null, cannot start videowall on this pc";
        closeInstanceDelayed();
        return;
    }

    connect(videoWall, &QnVideoWallResource::itemChanged, this, &QnWorkbenchVideoWallHandler::at_videoWall_itemChanged_activeMode);
    connect(videoWall, &QnVideoWallResource::itemRemoved, this, &QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode);

    bool master = m_videoWallMode.instanceGuid.isNull();
    if (master) {
        foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
            if (item.pcUuid != pcUuid || item.online)
                continue;

            QStringList arguments;
            arguments << QLatin1String("--videowall");
            arguments << m_videoWallMode.guid.toString();
            arguments << QLatin1String("--videowall-instance");
            arguments << item.uuid.toString();
            openNewWindow(arguments);
        }
        closeInstanceDelayed();
    } else {
        openVideoWallItem(videoWall);
    }
}

QnVideoWallItemIndexList QnWorkbenchVideoWallHandler::targetList() const {
    if (!workbench()->currentLayout()->resource())
        return QnVideoWallItemIndexList();

    QUuid currentId = workbench()->currentLayout()->resource()->getId();

    QnVideoWallItemIndexList indices;

    foreach (const QnResourcePtr &resource, resourcePool()->getResources()) {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall)
            continue;

        foreach(const QnVideoWallItem &item, videoWall->items()->getItems()) {
            if (item.layout == currentId && item.online)
                indices << QnVideoWallItemIndex(videoWall, item.uuid);
        }
    }

    return indices;
}

QnLayoutResourcePtr QnWorkbenchVideoWallHandler::findExistingResourceLayout(const QnResourcePtr &resource) const {
    if (!resource.dynamicCast<QnMediaResource>() && !resource.dynamicCast<QnMediaServerResource>())
        return QnLayoutResourcePtr();

    QUuid parentId = context()->user() ? context()->user()->getId() : QUuid();
    foreach(const QnLayoutResourcePtr &layout, qnResPool->getResourcesWithParentId(parentId).filtered<QnLayoutResource>()) {
        //TODO: #GDM #VW should we check name of this layout?
        if (layout->getItems().size() != 1)
            continue;
        QnLayoutItemData data = layout->getItems().values().first();
        QnResourcePtr existingResource;
        if(!data.resource.id.isNull()) {
            existingResource = qnResPool->getResourceById(data.resource.id);
        } else {
            existingResource = qnResPool->getResourceByUniqId(data.resource.path);
        }
        if (existingResource == resource)
            return layout;
    }

    return QnLayoutResourcePtr();
}

QnLayoutResourcePtr QnWorkbenchVideoWallHandler::constructLayout(const QnResourceList &resources) const {

    if (resources.size() == 1) {
        // If there is only one layout, return it
        if (QnLayoutResourcePtr layout = resources.first().dynamicCast<QnLayoutResource>())
            return layout;

        // If there is only one resource, try to find already created layout and return it
        if (QnLayoutResourcePtr layout = findExistingResourceLayout(resources.first()))
            return layout;
    }

    QnResourceList filtered;
    QMap<qreal, int> aspectRatios;
    qreal defaultAr = qnGlobals->defaultLayoutCellAspectRatio();

    auto addToFiltered = [&](const QnResourcePtr &resource) {
        if (!resource)
            return;

        if (resource.dynamicCast<QnMediaResource>())
            filtered << resource;
        else if (resource.dynamicCast<QnMediaServerResource>())
            filtered << resource;

        qreal ar = defaultAr;
        if (QnNetworkResourcePtr networkResource = resource.dynamicCast<QnNetworkResource>())
            ar = qnSettings->resourceAspectRatios().value(networkResource->getPhysicalId(), defaultAr);
        aspectRatios[ar] = aspectRatios[ar] + 1;
    };

    foreach (const QnResourcePtr &resource, resources) {
        if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            foreach (const QnLayoutItemData &item, layout->getItems()) {
                addToFiltered(qnResPool->getResourceByUniqId(item.resource.path));
            }
        } else {            
            addToFiltered(resource);
        }
    }
    
    if (filtered.isEmpty())
        return QnLayoutResourcePtr();

    qreal desiredAspectRatio = defaultAr;
    foreach (qreal ar, aspectRatios.keys()) {
        if (aspectRatios[ar] > aspectRatios[desiredAspectRatio])
            desiredAspectRatio = ar;
    }

    QnLayoutResourcePtr layout(new QnLayoutResource(qnResTypePool));
    layout->setId(QUuid::createUuid());
    if (filtered.size() == 1)
        layout->setName(generateUniqueLayoutName(context()->user(),
                                                 filtered.first()->getName(),
                                                 tr("%1 (%2)")
                                                 .arg(filtered.first()->getName())
                                                 .arg(lit("%1"))
                                                 ));
    else
        layout->setName(generateUniqueLayoutName(context()->user(), tr("New layout"), tr("New layout %1")));
    if(context()->user())
        layout->setParentId(context()->user()->getId());

    layout->setCellSpacing(0, 0);
    layout->setCellAspectRatio(desiredAspectRatio);
    layout->addFlags(Qn::local); // TODO: #Elric #EC2

    /* Calculate size of the resulting matrix. */
    const int matrixWidth = qMax(1, qRound(std::sqrt(desiredAspectRatio * filtered.size())));

    int i = 0;
    foreach (const QnResourcePtr &resource, filtered) {
        QnLayoutItemData item;
        item.flags = Qn::Pinned;
        item.uuid = QUuid::createUuid();
        item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
        item.resource.id = resource->getId();
        item.resource.path = resource->getUniqueId();
        layout->addItem(item);
        i++;
    }

    resourcePool()->addResource(layout);
    return layout;
}

/*------------------------------------ HANDLERS ------------------------------------------*/

void QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered() {
	QStringList usedNames;
    foreach(const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::videowall))
        usedNames << resource->getName().trimmed().toLower();
	
    //TODO: #GDM #VW refactor to corresponding dialog
    QString proposedName = generateUniqueString(usedNames, tr("Video Wall"), tr("Video Wall %1") );

    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Video Wall..."));
    dialog->setText(tr("Enter the name of the Video Wall to create:")); //TODO: #VW #TR
    dialog->setName(proposedName);
    dialog->setWindowModality(Qt::ApplicationModal);

    while (true) {
        if(!dialog->exec())
            return;

        proposedName = dialog->name().trimmed();
        if (proposedName.isEmpty())
            return;

        if (usedNames.contains(proposedName.toLower())) {
            QMessageBox::warning(
                mainWindow(),
                tr("Video Wall already exists"),
                tr("Video Wall with the same name already exists")
                );
            continue;
        }

        break;
    };

    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setId(QUuid::createUuid());
    videoWall->setName(proposedName);
    videoWall->setTypeByName(lit("Videowall"));

    connection2()->getVideowallManager()->save(videoWall,  this, 
        [this, videoWall]( int reqID, ec2::ErrorCode errorCode ) {
            Q_UNUSED(reqID);
            if (errorCode == ec2::ErrorCode::ok)
                return;

            //TODO: #GDM #VW make common place to call this dialog from different handlers
            QnResourceListDialog::exec(
                mainWindow(),
                QnResourceList() << videoWall,
                tr("Error"),
                tr("Could not save the following %n items to Server.", "", 1),
                QDialogButtonBox::Ok
                );
    } );
}

void QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered() {
    if (!context()->user())
        return;

    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVideoWallResourcePtr videoWall = parameters.resource().dynamicCast<QnVideoWallResource>();
    if(videoWall.isNull())
        return;

    QScopedPointer<QnAttachToVideowallDialog> dialog(new QnAttachToVideowallDialog(mainWindow()));
    dialog->loadFromResource(videoWall);
    if (!dialog->exec())
        return;
    dialog->submitToResource(videoWall);
    
    menu()->trigger(Qn::OpenVideoWallsReviewAction, QnActionParameters(videoWall));
    saveVideowallAndReviewLayout(videoWall);
}

void QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered() {
    QnVideoWallItemIndexList items = menu()->currentParameters(sender()).videoWallItems();

    QSet<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &index, items) {
        if (!index.isValid())
            continue;

        QnVideoWallItem existingItem = index.item();
        existingItem.layout = QUuid();
        index.videowall()->items()->updateItem(index.uuid(), existingItem);
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls);
}

void QnWorkbenchVideoWallHandler::at_resetVideoWallLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList items = parameters.videoWallItems();
    QnLayoutResourcePtr layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole,
                                                                          workbench()->currentLayout()->resource());

    resetLayout(items, layout);
}

void QnWorkbenchVideoWallHandler::at_deleteVideoWallItemAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVideoWallItemIndexList items = parameters.videoWallItems();

    QnResourceList resources;
    foreach(const QnVideoWallItemIndex &index, items) {
        if (!index.isValid())
            continue;
        QnResourcePtr proxyResource(new QnResource());
        proxyResource->setId(index.uuid());
        proxyResource->setName(index.item().name);
        qnResIconCache->setKey(proxyResource, QnResourceIconCache::VideoWallItem);
        resources.append(proxyResource);
    }

    QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
        mainWindow(),
        resources,
        tr("Delete Items"),
        tr("Are you sure you want to permanently delete these %n item(s)?", "", resources.size()),
        QDialogButtonBox::Yes | QDialogButtonBox::No
        );
    if(button != QDialogButtonBox::Yes)
        return;

    QSet<QnVideoWallResourcePtr> videoWalls;
    foreach (const QnVideoWallItemIndex &index, items) {
        if (!index.isValid())
            continue;
        index.videowall()->items()->removeItem(index.uuid());
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls, true);
}

void QnWorkbenchVideoWallHandler::at_startVideoWallAction_triggered() {
    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if(videoWall.isNull())
        return;
    
    startVideowallAndExit(videoWall);
}

void QnWorkbenchVideoWallHandler::at_stopVideoWallAction_triggered() {
    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if(videoWall.isNull())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::Exit);
    message.videoWallGuid = videoWall->getId();
    foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
        message.instanceGuid = item.uuid;
        connection2()->getVideowallManager()->sendControlMessage(message, this, []{});
    }
}

void QnWorkbenchVideoWallHandler::at_delayedOpenVideoWallItemAction_triggered() {
    m_videoWallMode.guid = menu()->currentParameters(sender()).argument<QUuid>(Qn::VideoWallGuidRole);
    m_videoWallMode.instanceGuid = menu()->currentParameters(sender()).argument<QUuid>(Qn::VideoWallItemGuidRole);
    m_videoWallMode.opening = true;
    submitDelayedItemOpen();
}

void QnWorkbenchVideoWallHandler::at_renameAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();

    QSet<QnVideoWallResourcePtr> videoWalls;
    switch (nodeType) {
    case Qn::VideoWallItemNode:
        {
            foreach (const QnVideoWallItemIndex &index, parameters.videoWallItems()) {
                if (!index.isValid())
                    continue;

                QnVideoWallItem existingItem = index.item();
                existingItem.name = name;
                index.videowall()->items()->updateItem(index.uuid(), existingItem);
                videoWalls << index.videowall();
            }
        }
        break;
    case Qn::VideoWallMatrixNode:
        {
            foreach (const QnVideoWallMatrixIndex &matrix, parameters.videoWallMatrices()) {
                if (!matrix.videowall())
                    continue;

                QnVideoWallMatrix existingMatrix = matrix.videowall()->matrices()->getItem(matrix.uuid());
                existingMatrix.name = name;
                matrix.videowall()->matrices()->updateItem(matrix.uuid(), existingMatrix);
                videoWalls << matrix.videowall();
            }
        }
        break;
    default:
        return;
    }

    saveVideowalls(videoWalls);
}

void QnWorkbenchVideoWallHandler::at_identifyVideoWallAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList indices = parameters.videoWallItems();
    if (indices.isEmpty()) {
        foreach (QnVideoWallResourcePtr videoWall, parameters.resources().filtered<QnVideoWallResource>()) {
            if(!videoWall)
                continue;
            foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
                indices << QnVideoWallItemIndex(videoWall, item.uuid);
            }
        }
    }

    QnVideoWallControlMessage message(QnVideoWallControlMessage::Identify);
    foreach (const QnVideoWallItemIndex &item, indices) {
        if (!item.isValid())
            continue;

        message.videoWallGuid = item.videowall()->getId();
        message.instanceGuid = item.uuid();
        connection2()->getVideowallManager()->sendControlMessage(message, this, []{});
    }
}

void QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnWorkbenchLayout *layout = NULL;

    QnVideoWallItemIndexList indices = parameters.videoWallItems();

    foreach (QnVideoWallItemIndex index, indices) {
        if (!index.isValid())
            continue;
        QnVideoWallItem item = index.item();
        if (item.layout.isNull())
            continue;
        QnLayoutResourcePtr layoutResource = qnResPool->getResourceById(item.layout).dynamicCast<QnLayoutResource>();
        if (!layoutResource)
            continue;

        layout = QnWorkbenchLayout::instance(layoutResource);
        if(!layout) {
            layout = new QnWorkbenchLayout(layoutResource, workbench());
            workbench()->addLayout(layout);
        }
        layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(item.uuid));
        layout->notifyTitleChanged();
    }

    if (layout)
        workbench()->setCurrentLayout(layout);
}

void QnWorkbenchVideoWallHandler::at_openVideoWallsReviewAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    foreach (const QnVideoWallResourcePtr &videoWall, parameters.resources().filtered<QnVideoWallResource>()) {

        QnWorkbenchLayout* existingLayout = QnWorkbenchLayout::instance(videoWall);
        if (existingLayout) {
            workbench()->setCurrentLayout(existingLayout);
            return;
        }

        /* Construct and add a new layout. */
        QnLayoutResourcePtr layout(new QnVideowallReviewLayoutResource(videoWall));
        if(context()->user())
            layout->setParentId(context()->user()->getId());
        if (accessController()->globalPermissions() & Qn::GlobalEditVideoWallPermission)
            layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission));

        QMap<ScreenWidgetKey, QnVideoWallItemIndexList> itemGroups;

        foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
            ScreenWidgetKey key(item.pcUuid, item.screenSnaps.screens());
            itemGroups[key].append(QnVideoWallItemIndex(videoWall, item.uuid));
        }

        foreach (const QnVideoWallItemIndexList &indices, itemGroups)
            addItemToLayout(layout, indices);

        resourcePool()->addResource(layout);

        menu()->trigger(Qn::OpenSingleLayoutAction, layout);

        // new layout should not be marked as changed
        saveVideowallAndReviewLayout(videoWall, layout);
    }
}

void QnWorkbenchVideoWallHandler::at_saveCurrentVideoWallReviewAction_triggered() {
    QnWorkbenchLayout* layout = workbench()->currentLayout();
    QnVideoWallResourcePtr videowall = layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!videowall)
        return;
    saveVideowallAndReviewLayout(videowall, layout->resource());
}

void QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVideoWallResourcePtr videowall = parameters.resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;
    saveVideowallAndReviewLayout(videowall);
}

void QnWorkbenchVideoWallHandler::at_dropOnVideoWallItemAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QUuid targetUuid = parameters.argument(Qn::VideoWallItemGuidRole).value<QUuid>();
    QnVideoWallItemIndex targetIndex = qnResPool->getVideoWallItemByUuid(targetUuid);
    if (targetIndex.isNull())
        return;
    QnLayoutResourcePtr currentLayout = qnResPool->getResourceById(targetIndex.item().layout).dynamicCast<QnLayoutResource>();

    Qt::KeyboardModifiers keyboardModifiers = parameters.argument<Qt::KeyboardModifiers>(Qn::KeyboardModifiersRole);
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();
    QnResourceList resources = parameters.resources();

    QnResourceList targetResources;
    QnVideoWallItemIndex sourceIndex;

    if (!videoWallItems.isEmpty()) {
        foreach (const QnVideoWallItemIndex &index, videoWallItems) {
            if (!index.isValid())
                continue;
            if (QnLayoutResourcePtr layout = qnResPool->getResourceById(index.item().layout).dynamicCast<QnLayoutResource>())
                targetResources << layout;
        }

        // dragging single videowall item causing swap (if Shift is not pressed)
        if (videoWallItems.size() == 1 && !videoWallItems.first().isNull() && !(Qt::ShiftModifier & keyboardModifiers) && currentLayout)
            sourceIndex = videoWallItems.first();
    } else {
        targetResources = resources;
    }

    // if Control pressed, add items to current layout
    if (Qt::ControlModifier & keyboardModifiers && currentLayout) {
            targetResources << currentLayout;
    }
    
    QnLayoutResourcePtr targetLayout = constructLayout(targetResources);

    if (currentLayout && !sourceIndex.isNull() && targetLayout)
        swapLayouts(targetIndex, targetLayout, sourceIndex, currentLayout);
    else if (targetLayout)
        resetLayout(QnVideoWallItemIndexList() << targetIndex, targetLayout);

}

void QnWorkbenchVideoWallHandler::at_pushMyScreenToVideowallAction_triggered() {
    if (!context()->user())
        return;

    QnVirtualCameraResourcePtr desktopCamera;

    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::desktop_camera)) {
        if (resource->getUniqueId() == QnAppServerConnectionFactory::clientGuid())
            desktopCamera = resource.dynamicCast<QnVirtualCameraResource>();    
    }
    if (!desktopCamera)
        return;

    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();

    foreach (const QnVideoWallItemIndex &index, videoWallItems) {
        parameters = QnActionParameters(desktopCamera);
        parameters.setArgument(Qn::VideoWallItemGuidRole, index.uuid());
        menu()->trigger(Qn::DropOnVideoWallItemAction, parameters);
    }
}

void QnWorkbenchVideoWallHandler::at_videowallSettingsAction_triggered() {
    QnVideoWallResourcePtr videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    bool shortcutsSupported = qnPlatform->shortcuts()->supported();

    QScopedPointer<QnVideowallSettingsDialog> dialog(new QnVideowallSettingsDialog(mainWindow()));
    dialog->loadFromResource(videowall);
    dialog->setShortcutsSupported(shortcutsSupported);
    if (shortcutsSupported)
        dialog->setCreateShortcut(shortcutExists(videowall));
    if (!dialog->exec())
        return;

    dialog->submitToResource(videowall);
    if (shortcutsSupported) {
        if (dialog->isCreateShortcut())
            createShortcut(videowall);
        else
            deleteShortcut(videowall);
    }

    saveVideowall(videowall);
}

void QnWorkbenchVideoWallHandler::at_saveVideowallMatrixAction_triggered() {
    QnVideoWallResourcePtr videowall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    QnVideoWallMatrix matrix;
    matrix.name = tr("New Matrix %1").arg(videowall->matrices()->getItems().size() + 1);
    matrix.uuid = QUuid::createUuid();

    foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
        if (item.layout.isNull() || !qnResPool->getResourceById(item.layout))
            continue;
        matrix.layoutByItem[item.uuid] = item.layout;
    }

    if (matrix.layoutByItem.isEmpty()) {
        QMessageBox::information(mainWindow(),
            tr("Invalid matrix"),
            tr("You have no layouts on the screens. Matrix cannot be saved.")); //TODO: #VW #TR
        return;
    }

    videowall->matrices()->addItem(matrix);
    saveVideowall(videowall);
}


void QnWorkbenchVideoWallHandler::at_loadVideowallMatrixAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

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
    foreach (QnVideoWallItem item, items) {
        if (!matrix.layoutByItem.contains(item.uuid))
            continue;

        QUuid layoutUuid = matrix.layoutByItem[item.uuid];
        if (!layoutUuid.isNull() && !qnResPool->getResourceById(layoutUuid))
            layoutUuid = QUuid();

        if (item.layout == layoutUuid)
            continue;

        item.layout = layoutUuid;
        videowall->items()->updateItem(item.uuid, item);
        hasChanges = true;
    }

    if (!hasChanges)
        return;
    
    saveVideowall(videowall);
}

void QnWorkbenchVideoWallHandler::at_deleteVideowallMatrixAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVideoWallMatrixIndexList matrices = parameters.videoWallMatrices();

    QnResourceList resources;
    foreach(const QnVideoWallMatrixIndex &index, matrices) {
        if (!index.videowall() || !index.videowall()->matrices()->hasItem(index.uuid()))
            continue;
        QnResourcePtr proxyResource(new QnResource());
        proxyResource->setId(index.uuid());
        proxyResource->setName(index.videowall()->matrices()->getItem(index.uuid()).name);
        qnResIconCache->setKey(proxyResource, QnResourceIconCache::VideoWallMatrix);
        resources.append(proxyResource);
    }

    QDialogButtonBox::StandardButton button = QnResourceListDialog::exec(
        mainWindow(),
        resources,
        tr("Delete Matrices"),
        tr("Are you sure you want to permanently delete these %n matrices?", "", resources.size()),
        QDialogButtonBox::Yes | QDialogButtonBox::No
        );
    if(button != QDialogButtonBox::Yes)
        return;

    QSet<QnVideoWallResourcePtr> videoWalls;
    foreach (const QnVideoWallMatrixIndex &matrix, matrices) {
        if (!matrix.videowall())
            continue;
        matrix.videowall()->matrices()->removeItem(matrix.uuid());
        videoWalls << matrix.videowall();
    }

    saveVideowalls(videoWalls); 
}

void QnWorkbenchVideoWallHandler::at_resPool_resourceAdded(const QnResourcePtr &resource) {
    QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
    if (!videoWall)
        return;

    connect(videoWall, &QnVideoWallResource::autorunChanged, this, [this] (const QnResourcePtr &resource) {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall || !videoWall->pcs()->hasItem(qnSettings->pcUuid()))
            return;
        QnVideowallAutoStarter(videoWall->getId(), this).setAutoStartEnabled(videoWall->isAutorun());
    });

    connect(videoWall, &QnVideoWallResource::pcAdded, this, [this] (const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
        if (pc.uuid != qnSettings->pcUuid())
            return;
        QnVideowallAutoStarter(videoWall->getId(), this).setAutoStartEnabled(videoWall->isAutorun());
    });

    connect(videoWall, &QnVideoWallResource::pcRemoved, this, [this] (const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
        if (pc.uuid != qnSettings->pcUuid())
            return;
        QnVideowallAutoStarter(videoWall->getId(), this).setAutoStartEnabled(false);
    });

    if (m_videoWallMode.active) {
        if (resource->getId() != m_videoWallMode.guid)
            return;

        if (m_videoWallMode.ready)
            return;
        m_videoWallMode.ready = true;
        submitDelayedItemOpen();
    } else {
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

void QnWorkbenchVideoWallHandler::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    if (m_videoWallMode.active) {
        if (resource->getId() != m_videoWallMode.guid)
            return;

        QnVideowallAutoStarter(resource->getId(), this).setAutoStartEnabled(false); //TODO: #GDM #VW clean nonexistent videowalls sometimes
        closeInstanceDelayed();
    } else {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall)
            return;
        disconnect(videoWall, NULL, this, NULL);
        QnVideowallAutoStarter(videoWall->getId(), this).setAutoStartEnabled(false); //TODO: #GDM #VW clean nonexistent videowalls sometimes

        QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
        if (layout && layout->resource())
            qnResPool->removeResource(layout->resource());
    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
        if (item.pcUuid != pc.uuid)
            continue;
        at_videoWall_itemAdded(videoWall, item);
    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
    //TODO: #GDM #VW implement screen size changes handling
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    QList<QnWorkbenchItem*> itemsToDelete;
    foreach(QnWorkbenchItem *workbenchItem, layout->items()) {
        QnLayoutItemData data = workbenchItem->data();
        QnVideoWallItemIndexList indices = data.dataByRole[Qn::VideoWallItemIndicesRole].value<QnVideoWallItemIndexList>();
        if (indices.isEmpty())
            continue;
        if (indices.first().item().pcUuid != pc.uuid)
             continue;
        itemsToDelete << workbenchItem;
    }

    foreach(QnWorkbenchItem *workbenchItem, itemsToDelete)
        layout->removeItem(workbenchItem);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {

    foreach (const QnPeerRuntimeInfo &info, QnRuntimeInfoManager::instance()->items()->getItems()) {
        if (info.data.videoWallInstanceGuid != item.uuid)
            continue;
       
        QnVideoWallItem updatedItem(item);
        updatedItem.online = true;
        videoWall->items()->updateItem(item.uuid, updatedItem);
        break;
    }

    updateReviewLayout(videoWall, item, ItemAction::Added);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    //TODO: #GDM #VW implement screen size changes handling

    updateControlLayout(videoWall, item, ItemAction::Changed);
    updateReviewLayout(videoWall, item, ItemAction::Changed);

    // check if item become online or offline
    if (item.uuid == workbench()->currentLayout()->data(Qn::VideoWallItemGuidRole).value<QUuid>())
        updateMode();
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    updateControlLayout(videoWall, item, ItemAction::Removed);
    updateReviewLayout(videoWall, item, ItemAction::Removed);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged_activeMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    if (videoWall->getId() != m_videoWallMode.guid || item.uuid != m_videoWallMode.instanceGuid)
        return;
    openVideoWallItem(videoWall);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    if (videoWall->getId() != m_videoWallMode.guid || item.uuid != m_videoWallMode.instanceGuid)
        return;
    closeInstanceDelayed();
}

void QnWorkbenchVideoWallHandler::at_eventManager_controlMessageReceived(const QnVideoWallControlMessage &message) {
    if (message.instanceGuid != m_videoWallMode.instanceGuid)
        return;

    // Ignore order for broadcast messages such as Exit or Identify
    if (!message.params.contains(sequenceKey)) {
        handleMessage(message);
        return;
    }

    QUuid controllerUuid = QUuid(message[pcUuidKey]);
    qint64 sequence = message[sequenceKey].toULongLong();

    // ControlStarted message set starting sequence number
    if (message.operation == QnVideoWallControlMessage::ControlStarted) {
        handleMessage(message, controllerUuid, sequence);
        restoreMessages(controllerUuid, sequence);
        return;
    }

    // all messages should go one-by-one
    //TODO: #GDM #VW what if one message is lost forever? timeout?
    if (!m_videoWallMode.sequenceByPcUuid.contains(controllerUuid) ||
            (sequence - m_videoWallMode.sequenceByPcUuid[controllerUuid] > 1)) {
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
    if (sequence < m_videoWallMode.sequenceByPcUuid[controllerUuid]) {
        qWarning() << "outdated control message" << message;
        return;
    }

    // Correct ordered message
    handleMessage(message, controllerUuid, sequence);

    //check for messages with next sequence
    restoreMessages(controllerUuid, sequence);
}

void QnWorkbenchVideoWallHandler::at_display_widgetAdded(QnResourceWidget* widget) {
    if (widget->resource()->flags() & Qn::sync) {
        if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)) {
            connect(mediaWidget, &QnMediaResourceWidget::motionSelectionChanged, this, &QnWorkbenchVideoWallHandler::at_widget_motionSelectionChanged);
            connect(mediaWidget, &QnMediaResourceWidget::dewarpingParamsChanged, this, &QnWorkbenchVideoWallHandler::at_widget_dewarpingParamsChanged);
        }
    }
}

void QnWorkbenchVideoWallHandler::at_display_widgetAboutToBeRemoved(QnResourceWidget* widget) {
    disconnect(widget, NULL, this, NULL);
}

void QnWorkbenchVideoWallHandler::at_widget_motionSelectionChanged() {
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

void QnWorkbenchVideoWallHandler::at_widget_dewarpingParamsChanged() {
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

void QnWorkbenchVideoWallHandler::at_workbench_currentLayoutAboutToBeChanged() {
    disconnect(workbench()->currentLayout(), NULL, this, NULL);
    setControlMode(false);
}

void QnWorkbenchVideoWallHandler::at_workbench_currentLayoutChanged() {
    connect(workbench()->currentLayout(), &QnWorkbenchLayout::dataChanged, this, &QnWorkbenchVideoWallHandler::at_workbenchLayout_dataChanged);
    updateMode();
}

void QnWorkbenchVideoWallHandler::at_workbench_itemChanged(Qn::ItemRole role) {
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

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *item) {
    connect(item, &QnWorkbenchItem::dataChanged,     this,   &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemAdded);
    message[uuidKey] = item->uuid().toString();
    message[resourceKey] = item->resourceUid();
    message[geometryKey] = QString::fromUtf8(QJson::serialized(item->geometry()));
    message[zoomRectKey] = QString::fromUtf8(QJson::serialized(item->zoomRect()));
    message[rotationKey] = QString::fromUtf8(QJson::serialized(item->rotation()));
    message[checkedButtonsKey] = QString::fromUtf8(QJson::serialized(item->data(Qn::ItemCheckedButtonsRole).toInt()));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem *item) {
    disconnect(item, &QnWorkbenchItem::dataChanged,     this,   &QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged);

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemRemoved);
    message[uuidKey] = item->uuid().toString();
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem) {
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

void QnWorkbenchVideoWallHandler::at_workbenchLayout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem) {
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

void QnWorkbenchVideoWallHandler::at_workbenchLayout_dataChanged(int role) {
    if (role == Qn::VideoWallItemGuidRole || role == Qn::VideoWallResourceRole) {
        updateMode();
        return;
    }

    if (!m_controlMode.active)
        return;

    QByteArray json;
    QVariant data = workbench()->currentLayout()->data(role);
    switch (role) {
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

void QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_dataChanged(int role) {

    if (!m_controlMode.active)
        return;

    QnWorkbenchItem* item = static_cast<QnWorkbenchItem *>(sender());
    QByteArray json;
    QVariant data = item->data(role);
    bool cached = false;

    switch (role) {
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
        qDebug() << "SENDER: Item"  << item->uuid() << debugRole(role) << "changed to" << value;
#endif
        QJson::serialize(value, &json);
        break;
    }

    case Qn::ItemZoomRectRole:
    {
        QRectF value = data.value<QRectF>();
#ifdef SENDER_DEBUG
        qDebug() << "SENDER: Item"  << item->uuid() << debugRole(role) << "changed to" << value;
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
        qDebug() << "SENDER: Item"  << item->uuid() << debugRole(role) << "changed to" << value;
#endif
        QJson::serialize(value, &json);
        break;
    }
    case Qn::ItemCheckedButtonsRole:
    {
        int value = data.toInt();
#ifdef SENDER_DEBUG
        qDebug() << "SENDER: Item"  << item->uuid() << debugRole(role) << "changed to" << value;
#endif
        QJson::serialize(value, &json);
        break;
    }
    case Qn::ItemFrameDistinctionColorRole:
    {
        QColor value = data.value<QColor>();
#ifdef SENDER_DEBUG
        qDebug() << "SENDER: Item"  << item->uuid() << debugRole(role) << "changed to" << value;
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
        qDebug() << "SENDER: cannot deserialize"  << item->uuid() << debugRole(role) << data;
#endif
        break;
    }

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemDataChanged);
    message[roleKey] = QString::number(role);
    message[uuidKey] = item->uuid().toString();
    message[valueKey] = QString::fromUtf8(json);
    sendMessage(message, cached);
}

void QnWorkbenchVideoWallHandler::at_navigator_positionChanged() {
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::NavigatorPositionChanged);
    message[positionKey] = QString::number(navigator()->position());
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_navigator_speedChanged() {
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::NavigatorSpeedChanged);
    message[speedKey] = QString::number(navigator()->speed());
    message[positionKey] = QString::number(navigator()->position());
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchStreamSynchronizer_runningChanged() {
    if (!m_controlMode.active)
        return;

    if (display()->isChangingLayout())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::SynchronizationChanged);
    message[valueKey] = QString::fromUtf8(QJson::serialized(context()->instance<QnWorkbenchStreamSynchronizer>()->state()));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_controlModeCacheTimer_timeout() {
    if (m_controlMode.cachedMessages.isEmpty())
        return;

    while (!m_controlMode.cachedMessages.isEmpty()) {
        QnVideoWallControlMessage message = m_controlMode.cachedMessages.takeLast();
        for (auto iter = m_controlMode.cachedMessages.begin(); iter != m_controlMode.cachedMessages.end();) {
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

QString QnWorkbenchVideoWallHandler::shortcutPath() {
    QString result = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (result.isEmpty())
        result = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return result;
}


bool QnWorkbenchVideoWallHandler::shortcutExists(const QnVideoWallResourcePtr &videowall) const {
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return false;

    return qnPlatform->shortcuts()->shortcutExists(destinationPath, videowall->getName());
}

bool QnWorkbenchVideoWallHandler::createShortcut(const QnVideoWallResourcePtr &videowall) {

    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return false;

    QStringList arguments;
    arguments << lit("--videowall");
    arguments << videowall->getId().toString();

    QUrl url = QnAppServerConnectionFactory::url();
    url.setUserName(QString());
    url.setPassword(QString());

    arguments << lit("--auth");
    arguments << QString::fromUtf8(url.toEncoded());

    return qnPlatform->shortcuts()->createShortcut(qApp->applicationFilePath(), destinationPath, videowall->getName(), arguments);
}

bool QnWorkbenchVideoWallHandler::deleteShortcut(const QnVideoWallResourcePtr &videowall) {
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return true;
    return qnPlatform->shortcuts()->deleteShortcut(destinationPath, videowall->getName());
}

void QnWorkbenchVideoWallHandler::saveVideowall(const QnVideoWallResourcePtr& videowall, bool saveLayout) {
    if (saveLayout && QnWorkbenchLayout::instance(videowall) )
        saveVideowallAndReviewLayout(videowall);
    else
        connection2()->getVideowallManager()->save(videowall, this, [] {});
}

void QnWorkbenchVideoWallHandler::saveVideowalls(const QSet<QnVideoWallResourcePtr> &videowalls, bool saveLayout) {
    foreach (const QnVideoWallResourcePtr &videowall, videowalls)
        saveVideowall(videowall, saveLayout);
}

bool QnWorkbenchVideoWallHandler::saveReviewLayout(const QnLayoutResourcePtr &layoutResource, std::function<void(int, ec2::ErrorCode)> callback) {
    return saveReviewLayout(QnWorkbenchLayout::instance(layoutResource), callback);   
}

bool QnWorkbenchVideoWallHandler::saveReviewLayout( QnWorkbenchLayout *layout, std::function<void(int, ec2::ErrorCode)> callback ) {
    if (!layout)
        return false;

    QSet<QnVideoWallResourcePtr> videowalls;
    foreach(QnWorkbenchItem *workbenchItem, layout->items()) {
        QnLayoutItemData data = workbenchItem->data();
        QnVideoWallItemIndexList indices = data.dataByRole[Qn::VideoWallItemIndicesRole].value<QnVideoWallItemIndexList>();
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
        videowall->pcs()->updateItem(item.pcUuid, pc);
        videowalls << videowall;
    }

    //TODO: #GDM #VW sometimes saving is not required
    foreach (const QnVideoWallResourcePtr &videowall, videowalls){
        connection2()->getVideowallManager()->save(videowall, this, 
            [this, callback]( int reqID, ec2::ErrorCode errorCode ) {
                callback(reqID, errorCode);
        } );
    }

    return !videowalls.isEmpty();
}

void QnWorkbenchVideoWallHandler::setItemOnline(const QUuid &instanceGuid, bool online) {
    Q_ASSERT(!instanceGuid.isNull());

    QnVideoWallItemIndex index = qnResPool->getVideoWallItemByUuid(instanceGuid);
    if (index.isNull())
        return;

    QnVideoWallItem item = index.item();
    item.online = online;
    index.videowall()->items()->updateItem(instanceGuid, item);
}

void QnWorkbenchVideoWallHandler::updateMainWindowGeometry(const QnScreenSnaps &screenSnaps) {
    QList<QRect> screens;
    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); ++i)
        screens << desktop->screenGeometry(i);
    QRect targetGeometry = screenSnaps.geometry(screens);
    bool equalsScreen = screens.contains(targetGeometry);

    if (equalsScreen) {
        mainWindow()->setGeometry(targetGeometry);
        menu()->action(Qn::EffectiveMaximizeAction)->setChecked(true);
    } else {
        menu()->action(Qn::EffectiveMaximizeAction)->setChecked(false);
        mainWindow()->setGeometry(targetGeometry);
    }       
}

void QnWorkbenchVideoWallHandler::updateControlLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action) {
    if (action == ItemAction::Changed) {

        // index to place updated layout
        int layoutIndex = -1;
        bool wasCurrent = false;

        // check if layout was changed or detached
        for (int i = 0; i < workbench()->layouts().size(); ++i) {
            QnWorkbenchLayout *layout = workbench()->layout(i);

            if (layout->data(Qn::VideoWallItemGuidRole).value<QUuid>() != item.uuid)
                continue;

            layout->notifyTitleChanged();   //in case of 'online' flag changed

            if (layout->resource() && item.layout == layout->resource()->getId())
                return; //everything is correct, no changes required

            wasCurrent = workbench()->currentLayout() == layout;
            layoutIndex = i;
            layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(QUuid()));
            workbench()->removeLayout(layout);
        }

        if (item.layout.isNull() || layoutIndex < 0)
            return;

        // add new layout if needed
        {
            QnLayoutResourcePtr layoutResource = qnResPool->getResourceById(item.layout).dynamicCast<QnLayoutResource>();
            if (!layoutResource)
                return;

            QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(layoutResource);
            if(!layout) 
                layout = new QnWorkbenchLayout(layoutResource, workbench());

            if (workbench()->layoutIndex(layout) < 0)
                workbench()->insertLayout(layout, layoutIndex);
            else 
                workbench()->moveLayout(layout, layoutIndex);

            layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(item.uuid));
            layout->notifyTitleChanged();
            if (wasCurrent)
                workbench()->setCurrentLayoutIndex(layoutIndex);
        }
    } else if (action == ItemAction::Removed) {
        for (int i = 0; i < workbench()->layouts().size(); ++i) {
            QnWorkbenchLayout *layout = workbench()->layout(i);

            if (layout->data(Qn::VideoWallItemGuidRole).value<QUuid>() != item.uuid)
                continue;
            layout->setData(Qn::VideoWallItemGuidRole, qVariantFromValue(QUuid()));
            layout->notifyTitleChanged();
        }
    }
}

void QnWorkbenchVideoWallHandler::updateReviewLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action) {
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videowall);
    if (!layout)
        return;

    auto findCurrentWorkbenchItem = [layout, &item]() -> QnWorkbenchItem* {
        foreach(QnWorkbenchItem *workbenchItem, layout->items()) {
            QnLayoutItemData data = workbenchItem->data();
            QnVideoWallItemIndexList indices = data.dataByRole[Qn::VideoWallItemIndicesRole].value<QnVideoWallItemIndexList>();
            if (indices.isEmpty())
                continue;

            // checking existing widgets containing target item
            foreach (const QnVideoWallItemIndex &index, indices) {
                if (index.uuid() != item.uuid) 
                    continue;
                return workbenchItem;
            }
        }
        return nullptr;
    };

    auto findTargetWorkbenchItem = [layout, &item]() -> QnWorkbenchItem* {
        QnWorkbenchItem *currentItem = nullptr;
        foreach(QnWorkbenchItem *workbenchItem, layout->items()) {
            QnLayoutItemData data = workbenchItem->data();
            QnVideoWallItemIndexList indices = data.dataByRole[Qn::VideoWallItemIndicesRole].value<QnVideoWallItemIndexList>();
            if (indices.isEmpty())
                continue;

            // checking existing widgets with same screen sets
            // take any other item on this widget
            int otherIdx = qnIndexOf(indices, [&item](const QnVideoWallItemIndex &idx){return idx.uuid() != item.uuid; });
            if ((otherIdx >= 0) && (indices[otherIdx].item().screenSnaps.screens() == item.screenSnaps.screens()))
                return workbenchItem;

            // our item is the only item on the widget, we can modify it as we want
            if (indices.size() == 1 && indices.first().item().uuid == item.uuid)
                currentItem = workbenchItem;

            //continue search to more sufficient widgets
        }
        return currentItem;
    };

    auto removeFromWorkbenchItem = [layout, &videowall, &item](QnWorkbenchItem* workbenchItem) {
        if (!workbenchItem)
            return;

        QnVideoWallItemIndexList indices = workbenchItem->data().dataByRole[Qn::VideoWallItemIndicesRole].value<QnVideoWallItemIndexList>();
        indices.removeAll(QnVideoWallItemIndex(videowall, item.uuid));
        if (indices.isEmpty())
            layout->removeItem(workbenchItem);
        else
            workbenchItem->setData(Qn::VideoWallItemIndicesRole, qVariantFromValue<QnVideoWallItemIndexList>(indices));
    };

    auto addToWorkbenchItem = [layout, &videowall, &item](QnWorkbenchItem* workbenchItem) {
        if (workbenchItem) {
            QnVideoWallItemIndexList indices = workbenchItem->data().dataByRole[Qn::VideoWallItemIndicesRole].value<QnVideoWallItemIndexList>();
            indices << QnVideoWallItemIndex(videowall, item.uuid);
            workbenchItem->setData(Qn::VideoWallItemIndicesRole, qVariantFromValue<QnVideoWallItemIndexList>(indices));
        } else {
            addItemToLayout(layout->resource(), QnVideoWallItemIndexList() << QnVideoWallItemIndex(videowall, item.uuid));
        }      
    };

    if (action == ItemAction::Added) {
        QnWorkbenchItem* workbenchItem = findTargetWorkbenchItem();
        addToWorkbenchItem(workbenchItem);
    } else if (action == ItemAction::Removed) {
        removeFromWorkbenchItem(findCurrentWorkbenchItem());
    } else if (action == ItemAction::Changed) {
        QnWorkbenchItem* workbenchItem = findCurrentWorkbenchItem();
        /* Find new widget for the item. */
        QnWorkbenchItem* newWorkbenchItem = findTargetWorkbenchItem();
        if (workbenchItem != newWorkbenchItem || !newWorkbenchItem) { // additional check in case both of them are null
            /* Remove item from the old widget. */ 
            removeFromWorkbenchItem(workbenchItem);
            addToWorkbenchItem(newWorkbenchItem);
        } 
        /*  else item left on the same widget, just do nothing, widget will update itself */

    }

}

bool QnWorkbenchVideoWallHandler::validateLicenses(const QString &detail) const {
    if (!m_licensesHelper->isValid()) {
        QMessageBox::warning(mainWindow(),
            tr("More licenses required"),
            detail + L'\n' +
            m_licensesHelper->getRequiredLicenseMsg(Qn::LC_VideoWall));
        return false;
    }
    return true;
}


void QnWorkbenchVideoWallHandler::saveVideowallAndReviewLayout(const QnVideoWallResourcePtr& videowall, const QnLayoutResourcePtr &layout) {
    QnWorkbenchLayout* workbenchLayout = layout
        ?  QnWorkbenchLayout::instance(layout)
        :  QnWorkbenchLayout::instance(videowall);

    QnLayoutResourcePtr workbenchResource = workbenchLayout->resource();

    auto callback =  [this, workbenchResource](int reqId, ec2::ErrorCode errorCode) {
        Q_UNUSED(reqId);
        if (!workbenchResource)
            return;

        snapshotManager()->setFlags(workbenchResource, snapshotManager()->flags(workbenchResource) & ~Qn::ResourceIsBeingSaved);
        if (errorCode != ec2::ErrorCode::ok)
            return;
        snapshotManager()->setFlags(workbenchResource, snapshotManager()->flags(workbenchResource) & ~Qn::ResourceIsChanged);
    };

    //TODO: #GDM #VW #LOW refactor common code to common place
    if (saveReviewLayout(workbenchLayout, callback)) {
        if (workbenchResource)
            snapshotManager()->setFlags(workbenchResource, snapshotManager()->flags(workbenchResource) | Qn::ResourceIsBeingSaved);
    } else { // e.g. workbench layout is empty
        connection2()->getVideowallManager()->save(videowall, this, callback);
    }
}


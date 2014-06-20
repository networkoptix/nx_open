#include "workbench_videowall_handler.h"

#include <QtCore/QProcess>
#include <QtCore/QRegExp>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

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

#include "version.h"

#define SENDER_DEBUG
#define RECEIVER_DEBUG

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

    void addItemToLayout(const QnLayoutResourcePtr &layout, const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc, const QList<int> screens) {
        int idx = screens.first();
        int firstScreen = qnIndexOf(pc.screens, [idx](const QnVideoWallPcData::PcScreen &screen) {return screen.index == idx;});
        if (firstScreen < 0)
            return;

        QnLayoutItemData itemData;
        itemData.uuid = QUuid::createUuid();
        itemData.combinedGeometry = pc.screens[firstScreen].layoutGeometry;
        if (itemData.combinedGeometry.isValid())
            itemData.flags = Qn::Pinned;
        else
            itemData.flags = Qn::PendingGeometryAdjustment;
        itemData.resource.id = videoWall->getId();
        itemData.resource.path = videoWall->getUniqueId();
        itemData.dataByRole[Qn::VideoWallPcGuidRole] = qVariantFromValue<QUuid>(pc.uuid);
        itemData.dataByRole[Qn::VideoWallPcScreenIndicesRole] = qVariantFromValue<QList<int> >(screens);
        layout->addItem(itemData);
    }

    const int identifyTimeout  = 5000;
    const int identifyFontSize = 100;

    const int cacheMessagesTimeoutMs = 500;

    const qreal defaultReviewAR = 1920.0 / 1080.0;
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
        QUrl url = qnSettings->lastUsedConnection().url;
        url.setUserName(QString());
        url.setPassword(QString());
        arguments << QLatin1String("--auth");
        arguments << QLatin1String(url.toEncoded());

        QFileInfo clientFile = QFileInfo(qApp->applicationFilePath());
        QString result = toRegistryFormat(clientFile.canonicalFilePath()) + L' ' + arguments.join(L' ');
        return result;
    }

    virtual QString autoStartKey() const override { return lit(QN_APPLICATION_NAME) + L' ' + m_videoWallUuid.toString(); }
private:
    QUuid m_videoWallUuid;
};

class QnVideowallReviewLayoutResource: public QnLayoutResource {
public:
    QnVideowallReviewLayoutResource(const QnVideoWallResourcePtr &videowall):
        QnLayoutResource()
    {
        setId(QUuid::createUuid());
        addFlags(QnResource::local);
        setTypeByName(lit("Layout"));
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
    QnWorkbenchContextAware(parent)
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

QnWorkbenchVideoWallHandler::ScreenSnap QnWorkbenchVideoWallHandler::findNearest(const QList<ScreenSnap> &list, int value) {

    QList<ScreenSnap>::ConstIterator i = qLowerBound(list.constBegin(), list.constEnd(), value);
    if (i == list.constEnd())
        return list.last();

    if (i->value == value)
        return *i;

    int idx = (i - list.constBegin());
    if (idx <= 0)
        return list.first();

    if (idx >= list.size())
        return list.last();

    ScreenSnap prev = list[idx - 1];
    ScreenSnap next = list[idx];
    if (qAbs(value - prev.value) <= qAbs(value - next.value))
        return prev;
    return next;
}

QnWorkbenchVideoWallHandler::ScreenSnap QnWorkbenchVideoWallHandler::findEdge(const QList<QnWorkbenchVideoWallHandler::ScreenSnap> &snaps, QList<int> screens, bool backward) {

    if (backward) {
        foreach(const ScreenSnap &snap, snaps | boost::adaptors::reversed) {
            if (snap.intermidiate || !screens.contains(snap.index))
                continue;
            return snap;
        }
    } else {
        foreach(const ScreenSnap &snap, snaps) {
            if (snap.intermidiate || !screens.contains(snap.index))
                continue;
            return snap;
        }
    }

    return ScreenSnap();
}

QList<int> QnWorkbenchVideoWallHandler::getScreensByItem(const ScreenSnaps &snaps, const QRect &source) {

    ScreenSnaps joined = snaps.joined();

    ScreenSnap left     = findNearest(joined.left, source.left());
    ScreenSnap top      = findNearest(joined.top, source.top());
    ScreenSnap right    = findNearest(joined.right, source.right());
    ScreenSnap bottom   = findNearest(joined.bottom, source.bottom());
    QSet<int> screenIndices;
    screenIndices << left.index << top.index << right.index << bottom.index;

    QList<int> result = screenIndices.values();
    foreach (int index, result) {   //check if it is really one screen
        ScreenSnaps filtered = snaps.filtered(index);
        if (findNearest(filtered.left, source.left()).value != left.value)
            continue;
        if (findNearest(filtered.top, source.top()).value != top.value)
            continue;
        if (findNearest(filtered.right, source.right()).value != right.value)
            continue;
        if (findNearest(filtered.bottom, source.bottom()).value != bottom.value)
            continue;

        return QList<int>() << index;
    }
    return result;
}

QRect QnWorkbenchVideoWallHandler::calculateSnapGeometry(const QList<QnVideoWallPcData::PcScreen> &screens, const QRect &source) {
    if (screens.isEmpty())
        return source;

    ScreenSnaps localSnaps = calculateSnaps(qnSettings->pcUuid(), screens);

    ScreenSnap left, top, right, bottom;

    // multiple calculations are not important because function is used very rare and datasets are small
    QList<int> screenIndices = getScreensByItem(localSnaps, source);
    if (screenIndices.size() > 1) { // if client uses some screens it should fill them completely
        left    = findEdge(localSnaps.left,     screenIndices);
        top     = findEdge(localSnaps.top,      screenIndices);
        right   = findEdge(localSnaps.right,    screenIndices, true);
        bottom  = findEdge(localSnaps.bottom,   screenIndices, true);
    } else if (!screenIndices.isEmpty()) {  //safety check
        ScreenSnaps joined = localSnaps.joined();
        left    = findNearest(joined.left,      source.left());
        top     = findNearest(joined.top,       source.top());
        right   = findNearest(joined.right,     source.right());
        bottom  = findNearest(joined.bottom,    source.bottom());
    } else {
        return source;
    }
    //TODO: #GDM #VW check edges validity
    //TODO: #GDM #VW check that screens are not in use already

    return QRect(QPoint(left.value, top.value), QPoint(right.value, bottom.value));

}

void QnWorkbenchVideoWallHandler::attachLayout(const QnVideoWallResourcePtr &videoWall, const QnLayoutResourcePtr &layout, const QnVideowallAttachSettings &settings) {
    QDesktopWidget* desktop = qApp->desktop();
    QList<QnVideoWallPcData::PcScreen> localScreens;
    QRect unitedGeometry;
    for (int i = 0; i < desktop->screenCount(); i++) {
        QnVideoWallPcData::PcScreen screen;
        screen.index = i;
        screen.desktopGeometry = desktop->screenGeometry(i);
        unitedGeometry = unitedGeometry.united(screen.desktopGeometry);
        localScreens << screen;
    }
    int currentScreen = desktop->screenNumber(mainWindow());
    QUuid pcUuid = qnSettings->pcUuid();
    
    auto newItem = [&]() {
        QnVideoWallItem result;

        // if layout can be attached right now, do it
        if (layout && !snapshotManager()->isLocal(layout))
            result.layout = layout->getId();

        result.name = generateUniqueString([&videoWall] () {
            QStringList used;
            foreach (const QnVideoWallItem &item, videoWall->items()->getItems())
                used << item.name;
            return used;
        }(), tr("Screen"), tr("Screen %1") );
        result.pcUuid = pcUuid;
        result.uuid = QUuid::createUuid();
        return result;
    };

    QnVideoWallItem item = newItem();

    switch (settings.attachMode) {
    case QnVideowallAttachSettings::AttachAll:
        item.geometry = unitedGeometry;
        break;
    case QnVideowallAttachSettings::AttachScreen:
        item.geometry = desktop->screenGeometry(currentScreen);
        break;
    case QnVideowallAttachSettings::AttachWindow:
        item.geometry = calculateSnapGeometry(localScreens, mainWindow()->geometry());
        break;
    default:
        break;
    }

  //  action(Qn::EffectiveMaximizeAction)->setChecked(false);
  //  mainWindow()->setGeometry(item.geometry);   // WYSIWYG

    videoWall->items()->addItem(item);
    QnVideoWallItemIndexList items;
    items << QnVideoWallItemIndex(videoWall, item.uuid);

    if (settings.autoFill) {
        switch (settings.attachMode) {
        case QnVideowallAttachSettings::AttachScreen: 
        {
            for (int i = 0; i < desktop->screenCount(); i++) {
                if (i == currentScreen)
                    continue;
                QnVideoWallItem fillItem = newItem();
                fillItem.geometry = desktop->screenGeometry(i);
                videoWall->items()->addItem(fillItem);
                items << QnVideoWallItemIndex(videoWall, fillItem.uuid);
            }
            break;
        }
        case QnVideowallAttachSettings::AttachWindow:
        {
            int w = item.geometry.width();
            int h = item.geometry.height();
            int xOffset = unitedGeometry.left();
            int yOffset = unitedGeometry.top();
            for (int x = 0; x < qRound((qreal)unitedGeometry.width() / w); x++) {
                for (int y = 0; y < qRound((qreal)unitedGeometry.height() / h); y++) {
                    QRect geometry = calculateSnapGeometry(localScreens, QRect(xOffset + x*w, yOffset + y*h, w, h));
                    if (geometry == item.geometry)
                        continue;   //TODO: #GDM #VW check overlapping with existing items
                    QnVideoWallItem fillItem = newItem();
                    fillItem.geometry = geometry;
                    videoWall->items()->addItem(fillItem);
                    items << QnVideoWallItemIndex(videoWall, fillItem.uuid);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    QnVideoWallPcData pcData;
    pcData.uuid = pcUuid;
    pcData.screens = localScreens;

    if (!videoWall->pcs()->hasItem(pcUuid))
        videoWall->pcs()->addItem(pcData);
    else
        videoWall->pcs()->updateItem(pcUuid, pcData);

    // If layout should be saved, attach it after videowall saving.
    connection2()->getVideowallManager()->save(videoWall,  this, 
        [this, items, layout, videoWall]( int reqID, ec2::ErrorCode errorCode ) {
            Q_UNUSED(reqID);
            if (errorCode != ec2::ErrorCode::ok)
                return;

            if (items.isEmpty())
                return;

            bool updateLayout = !layout.isNull();
            foreach(const QnVideoWallItemIndex &index, items) {
                if (index.isNull())
                    continue;
                QnVideoWallResourcePtr videowall = index.videowall();
                if (!videowall->items()->hasItem(index.uuid()))
                    continue;
                if (!videowall->items()->getItem(index.uuid()).layout.isNull()) {
                    updateLayout = false;
                    break;
                }
            }
            if (updateLayout)
                resetLayout(items, layout);
    } );

    menu()->trigger(Qn::OpenVideoWallsReviewAction, QnActionParameters(videoWall));
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
    QnLayoutResourceList unsavedLayouts;
    if (snapshotManager()->isLocal(firstLayout) || snapshotManager()->isModified(firstLayout))
        unsavedLayouts << firstLayout;

    if (snapshotManager()->isLocal(secondLayout) || snapshotManager()->isModified(secondLayout))
        unsavedLayouts << secondLayout;

    auto swap = [this](const QnVideoWallItemIndex firstIndex, const QnLayoutResourcePtr &firstLayout, const QnVideoWallItemIndex &secondIndex, const QnLayoutResourcePtr &secondLayout) {
        QnVideoWallItem firstItem = firstIndex.videowall()->items()->getItem(firstIndex.uuid());
        firstItem.layout = firstLayout->getId();
        firstIndex.videowall()->items()->updateItem(firstIndex.uuid(), firstItem);

        QnVideoWallItem secondItem = secondIndex.videowall()->items()->getItem(secondIndex.uuid());
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

void QnWorkbenchVideoWallHandler::updateItemsLayout(const QnVideoWallItemIndexList &items, const QnId &layoutId) {
    QSet<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &item, items) {
        if (!item.videowall())
            continue;

        QnVideoWallItem existingItem = item.videowall()->items()->getItem(item.uuid());
        if (existingItem.layout == layoutId)
            continue;

        existingItem.layout = layoutId;
        item.videowall()->items()->updateItem(item.uuid(), existingItem);
        videoWalls << item.videowall();
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


bool QnWorkbenchVideoWallHandler::canClose() {
    if (!context()->user())
        return true;
    //TODO: #GDM #VW implement real check. Think about circular dependencies
    /*
    if (businessRulesDialog() && businessRulesDialog()->isVisible()) {
        businessRulesDialog()->activateWindow();
        if (!businessRulesDialog()->canClose())
            return;
        businessRulesDialog()->hide();
    }

    menu()->trigger(Qn::ClearCameraSettingsAction);
    if(!context()->instance<QnWorkbenchLayoutsHandler>()->closeAllLayouts(true))
        return;*/
    return true;
}


void QnWorkbenchVideoWallHandler::startVideowallAndExit(const QnVideoWallResourcePtr &videoWall) {
    if (!canStartVideowall(videoWall)) {
        QMessageBox::warning(mainWindow(),
            tr("Error"),
            tr("There are no offline videowall items attached to this pc.")); //TODO: #VW #TR
        return;
    }

    bool doNotAskAgain = false;
    int startMode = qnSettings->videoWallStartMode();
    QDialogButtonBox::StandardButton button = QDialogButtonBox::Yes; 
    if (startMode < 0) {
        button = QnCheckableMessageBox::question(
            mainWindow(),
            tr("Switch to Video Wall Mode..."),
            tr("Client will be closed and reopened as Video Wall."), //TODO: #VW #TR
            QString(),
            &doNotAskAgain,
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
            QDialogButtonBox::Yes,
            QDialogButtonBox::Cancel
            );
    } else {
        button = static_cast<QDialogButtonBox::StandardButton>(startMode);
    }

    if (button == QDialogButtonBox::Cancel)
        return;

    if (button == QDialogButtonBox::Yes) {
        if (canClose())
            closeInstanceDelayed();
    }
    
    if (doNotAskAgain)
        qnSettings->setVideoWallStartMode(static_cast<int>(button));


    QUuid pcUuid = qnSettings->pcUuid();
    foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
        if (item.pcUuid != pcUuid || item.online)
            continue;

        QStringList arguments;
        arguments << QLatin1String("--videowall");
        arguments << videoWall->getId().toString();
        arguments << QLatin1String("--videowall-instance");
        arguments << item.uuid.toString();
        openNewWindow(arguments);
    }
}

void QnWorkbenchVideoWallHandler::openNewWindow(const QStringList &args) {
    QStringList arguments = args;

    QUrl url = qnSettings->lastUsedConnection().url;
    url.setUserName(QString());
    url.setPassword(QString());

    arguments << QLatin1String("--auth");
    arguments << QLatin1String(url.toEncoded());

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
    mainWindow()->setGeometry(item.geometry);

    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); i++) {
        QRect screen = desktop->screenGeometry(i);
        if (item.geometry == screen)
            menu()->action(Qn::EffectiveMaximizeAction)->trigger();
    }

    QnLayoutResourcePtr layout = qnResPool->getResourceById(item.layout).dynamicCast<QnLayoutResource>();
    if (layout)
        menu()->trigger(Qn::OpenSingleLayoutAction, layout);
}

void QnWorkbenchVideoWallHandler::closeInstanceDelayed() {
    QTimer::singleShot(10, action(Qn::ExitAction), SLOT(trigger()));
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
        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStarted));


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
    }
}

void QnWorkbenchVideoWallHandler::updateMode() {
    QnWorkbenchLayout* layout = workbench()->currentLayout();

    QUuid itemUuid = layout->data(Qn::VideoWallItemGuidRole).value<QUuid>();
    bool control = !itemUuid.isNull();
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
        bool first = true;

        foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
            if (item.pcUuid != pcUuid)
                continue;

            if (first) {
                first = false;
                m_videoWallMode.instanceGuid = item.uuid;
                openVideoWallItem(videoWall);
                continue;
            }

            QStringList arguments;
            arguments << QLatin1String("--videowall");
            arguments << m_videoWallMode.guid.toString();
            arguments << QLatin1String("--videowall-instance");
            arguments << item.uuid.toString();
            openNewWindow(arguments);
        }
    } else {
        openVideoWallItem(videoWall);
    }
}

QnWorkbenchVideoWallHandler::ScreenSnaps QnWorkbenchVideoWallHandler::calculateSnaps(const QUuid &pcUuid, const QList<QnVideoWallPcData::PcScreen> &screens) {
    if (m_screenSnapsByUuid.contains(pcUuid))
        return m_screenSnapsByUuid[pcUuid];

    ScreenSnaps result;
    QList<ScreenSnap> left, right, top, bottom;

    foreach (const QnVideoWallPcData::PcScreen &screen, screens) {
        QRect geom = screen.desktopGeometry;
        int w = geom.width() / 2;
        int h = geom.height() / 2;

        left     << ScreenSnap(screen.index, geom.left(), false)      << ScreenSnap(screen.index, geom.left() + w + 1, true);
        right    << ScreenSnap(screen.index, geom.left() + w, true)   << ScreenSnap(screen.index, geom.right(), false);
        top      << ScreenSnap(screen.index, geom.top(), false)       << ScreenSnap(screen.index, geom.top() + h + 1, true);
        bottom   << ScreenSnap(screen.index, geom.top() + h, true)    << ScreenSnap(screen.index, geom.bottom(), false);
    }

    qSort(left);
    result.left = left;

    qSort(right);
    result.right = right;

    qSort(top);
    result.top = top;

    qSort(bottom);
    result.bottom = bottom;

    m_screenSnapsByUuid[pcUuid] = result;
    return result;
}

QnVideoWallItemIndexList QnWorkbenchVideoWallHandler::targetList() const {
    if (!workbench()->currentLayout()->resource())
        return QnVideoWallItemIndexList();

    QnId currentId = workbench()->currentLayout()->resource()->getId();

    QnVideoWallItemIndexList indexes;

    foreach (const QnResourcePtr &resource, resourcePool()->getResources()) {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall)
            continue;

        foreach(const QnVideoWallItem &item, videoWall->items()->getItems()) {
            if (item.layout == currentId)
                indexes << QnVideoWallItemIndex(videoWall, item.uuid);
        }
    }

    return indexes;
}

QnLayoutResourcePtr QnWorkbenchVideoWallHandler::findExistingResourceLayout(const QnResourcePtr &resource) const {
    if (!resource.dynamicCast<QnMediaResource>() && !resource.dynamicCast<QnMediaServerResource>())
        return QnLayoutResourcePtr();

    QnId parentId = context()->user() ? context()->user()->getId() : QnId();
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

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QUuid::createUuid());
    layout->setTypeByName(lit("Layout"));
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
    layout->addFlags(QnResource::local); // TODO: #Elric #EC2

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
    //TODO: #GDM #VW refactor to corresponding dialog
    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Video Wall..."));
    dialog->setText(tr("Enter the name of the Video Wall to create:")); //TODO: #VW #TR
    dialog->setName(
        generateUniqueString([] () {
                QStringList used;
                foreach(const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::videowall))
                    used << resource->getName();
                return used;
            }(), tr("Video Wall"), tr("Video Wall %1") )
    );
    dialog->setWindowModality(Qt::ApplicationModal);

    if(!dialog->exec())
        return;

    QString name = dialog->name();
    if (name.isEmpty())
        return;

    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setId(QUuid::createUuid());
    videoWall->setName(name);
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
                tr("Could not save the following %n items to Enterprise Controller.", "", 1),
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

    // Suggest any of the current user's layouts
    QnLayoutResourceList layouts;
    foreach (const QnLayoutResourcePtr &layout, qnResPool->getResourcesWithParentId(context()->user()->getId()).filtered<QnLayoutResource>())
        if (!snapshotManager()->isFile(layout) && !layout->data().contains(Qn::VideoWallResourceRole))
            layouts << layout;

    qSort(layouts.begin(), layouts.end(),  [](const QnLayoutResourcePtr &l, const QnLayoutResourcePtr &r) {
        return naturalStringCaseInsensitiveLessThan(l->getName(), r->getName());
    });

     QnLayoutResourcePtr currentLayout = workbench()->currentLayout()->resource();
     bool canClone = layouts.contains(currentLayout);

    // If selected layout is invalid, suggest current workbench layout as a default value
    if (m_attachSettings.layoutId.isNull() || qnIndexOf(layouts, [&](const QnLayoutResourcePtr &layout) {return layout->getId() == m_attachSettings.layoutId;}) < 0)
        m_attachSettings.layoutId = currentLayout->getId();

    bool shortcutsSupported = qnPlatform->shortcuts()->supported();

    QScopedPointer<QnAttachToVideowallDialog> dialog(new QnAttachToVideowallDialog(mainWindow()));
    dialog->loadLayoutsList(layouts);
    dialog->setCanClone(canClone);
    dialog->loadSettings(m_attachSettings);
    dialog->setShortcutsSupported(shortcutsSupported);
    if (shortcutsSupported)
        dialog->setCreateShortcut(!shortcutExists(videoWall));
    if (!dialog->exec())
        return;
    m_attachSettings = dialog->settings();

    QnLayoutResourcePtr targetLayout;

    switch (m_attachSettings.layoutMode) {
    case QnVideowallAttachSettings::LayoutCustom:
    {
        int idx = qnIndexOf(layouts, [&](const QnLayoutResourcePtr &l){ return l->getId() == m_attachSettings.layoutId; });
        if (idx >= 0)
            targetLayout = layouts[idx];
        break;
    }
    case QnVideowallAttachSettings::LayoutClone:
    {
        if (!canClone)
            break;

        targetLayout = currentLayout->clone();
        targetLayout->setName(generateUniqueLayoutName(context()->user(), tr("Video Wall Layout"),  tr("Video Wall Layout %1")));
        targetLayout->setParentId(context()->user()->getId());
        targetLayout->addFlags(QnResource::local);
        targetLayout->setCellSpacing(QSizeF(0.0, 0.0));
        targetLayout->setUserCanEdit(true);
        resourcePool()->addResource(targetLayout);
        break;
    }
    default:
        break;
    }

    attachLayout(videoWall, targetLayout, m_attachSettings);

    if (shortcutsSupported && dialog->isCreateShortcut())
        createShortcut(videoWall);
}

void QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered() {
    QnVideoWallItemIndexList items = menu()->currentParameters(sender()).videoWallItems();

    QSet<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &item, items) {
        if (!item.videowall())
            continue;

        QnVideoWallItem existingItem = item.videowall()->items()->getItem(item.uuid());
        existingItem.layout = QnId();
        item.videowall()->items()->updateItem(item.uuid(), existingItem);
        videoWalls << item.videowall();
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
        if (!index.videowall() || !index.videowall()->items()->hasItem(index.uuid()))
            continue;
        QnResourcePtr proxyResource(new QnResource());
        proxyResource->setId(index.uuid());
        proxyResource->setName(index.videowall()->items()->getItem(index.uuid()).name);
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
        if (!index.videowall())
            continue;
        index.videowall()->items()->removeItem(index.uuid());
        videoWalls << index.videowall();
    }

    saveVideowalls(videoWalls);
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
            QnVideoWallItemIndexList items = parameters.videoWallItems();
            if (items.isEmpty())
                return;

            foreach (const QnVideoWallItemIndex &item, items) {
                if (!item.videowall())
                    continue;

                QnVideoWallItem existingItem = item.videowall()->items()->getItem(item.uuid());
                existingItem.name = name;
                item.videowall()->items()->updateItem(item.uuid(), existingItem);
                videoWalls << item.videowall();
            }
        }
        break;
    case Qn::VideoWallMatrixNode:
        {
            QnVideoWallMatrixIndexList matrices = parameters.videoWallMatrices();
            if (matrices.isEmpty())
                return;

            foreach (const QnVideoWallMatrixIndex &matrix, matrices) {
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
        break;
    }

    saveVideowalls(videoWalls);
}

void QnWorkbenchVideoWallHandler::at_identifyVideoWallAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList items = parameters.videoWallItems();
    if (items.isEmpty()) {
        foreach (QnVideoWallResourcePtr videoWall, parameters.resources().filtered<QnVideoWallResource>()) {
            if(!videoWall)
                continue;
            foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
                items << QnVideoWallItemIndex(videoWall, item.uuid);
            }
        }
    }

    QnVideoWallControlMessage message(QnVideoWallControlMessage::Identify);
    foreach (const QnVideoWallItemIndex &item, items) {
        message.videoWallGuid = item.videowall()->getId();
        message.instanceGuid = item.uuid();
        connection2()->getVideowallManager()->sendControlMessage(message, this, []{});
    }
}

void QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnWorkbenchLayout *layout = NULL;

    QnVideoWallItemIndexList items = parameters.videoWallItems();
    foreach (QnVideoWallItemIndex index, items) {
        if (!index.videowall())
            continue;
        QnVideoWallItem item = index.videowall()->items()->getItem(index.uuid());
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


        foreach (const QnVideoWallPcData &pc, videoWall->pcs()->getItems()) {
            QSet<int> usedScreens;

            foreach (const QnVideoWallItem &item, videoWall->items()->getItems()) {
                if (item.pcUuid != pc.uuid)
                    continue;

                QList<int> screens = getScreensByItem(calculateSnaps(pc.uuid, pc.screens), item.geometry);
                if (screens.size() == 0)
                    continue;
                qSort(screens);

                bool skip = false;
                foreach (int index, screens) {
                    skip |= usedScreens.contains(index);
                    usedScreens << index;
                }

                if (skip)
                    continue;

                addItemToLayout(layout, videoWall, pc, screens);
            }
        }

        resourcePool()->addResource(layout);
        menu()->trigger(Qn::OpenSingleLayoutAction, layout);
    }
}

void QnWorkbenchVideoWallHandler::at_saveCurrentVideoWallReviewAction_triggered() {
    QnWorkbenchLayout* layout = workbench()->currentLayout();
    QnVideoWallResourcePtr videowall = layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!videowall)
        return;
    menu()->trigger(Qn::SaveVideoWallReviewAction, QnActionParameters(videowall).withArgument(Qn::LayoutResourceRole, layout->resource()));
}

void QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVideoWallResourcePtr videowall = parameters.resource().dynamicCast<QnVideoWallResource>();
    if (!videowall)
        return;

    QnWorkbenchLayout* layout = NULL;
    QnLayoutResourcePtr layoutResource = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole);
    if (layoutResource)
        layout = QnWorkbenchLayout::instance(layoutResource);
    else
        layout = QnWorkbenchLayout::instance(videowall);
    
    if (!layout) 
        return;

    if (!layoutResource)
        layoutResource = layout->resource();

    //TODO: #GDM #VW #LOW refactor common code to common place
    if (saveReviewLayout(layoutResource, [this, layoutResource](int reqId, ec2::ErrorCode errorCode) {
        Q_UNUSED(reqId);
        snapshotManager()->setFlags(layoutResource, snapshotManager()->flags(layoutResource) & ~Qn::ResourceIsBeingSaved);
        if (errorCode != ec2::ErrorCode::ok)
            return;
        snapshotManager()->setFlags(layoutResource, snapshotManager()->flags(layoutResource) & ~Qn::ResourceIsChanged);
    }))
        snapshotManager()->setFlags(layoutResource, snapshotManager()->flags(layoutResource) | Qn::ResourceIsBeingSaved);
}

void QnWorkbenchVideoWallHandler::at_dropOnVideoWallItemAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QUuid targetUuid = parameters.argument(Qn::VideoWallItemGuidRole).value<QUuid>();
    QnVideoWallItemIndex targetIndex = qnResPool->getVideoWallItemByUuid(targetUuid);
    if (targetIndex.isNull())
        return;
    QnLayoutResourcePtr currentLayout = qnResPool->getResourceById(targetIndex.videowall()->items()->getItem(targetIndex.uuid()).layout).dynamicCast<QnLayoutResource>();

    Qt::KeyboardModifiers keyboardModifiers = parameters.argument<Qt::KeyboardModifiers>(Qn::KeyboardModifiersRole);
    QnVideoWallItemIndexList videoWallItems = parameters.videoWallItems();
    QnResourceList resources = parameters.resources();

    QnResourceList targetResources;
    QnVideoWallItemIndex sourceIndex;

    if (!videoWallItems.isEmpty()) {
        foreach (const QnVideoWallItemIndex &index, videoWallItems) {
            if (index.isNull())
                continue;
            if (QnLayoutResourcePtr layout = qnResPool->getResourceById(index.videowall()->items()->getItem(index.uuid()).layout).dynamicCast<QnLayoutResource>())
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
    // Desktop_camera_{e87e9b3d-facf-4870-abef-455861829ed3}_admin
    //TODO: #GDM #VW ask Roma to do some more stable way to find correct desktop camera
    QRegExp desktopCameraNameRegExp(QString(lit("Desktop_camera_\\{.{36,36}\\}_%1")).arg(context()->user()->getName()));
    QnVirtualCameraResourcePtr desktopCamera;

    foreach (const QnResourcePtr &resource, qnResPool->getAllCameras(QnResourcePtr())) {
        QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            continue;
        if (!desktopCameraNameRegExp.exactMatch(camera->getPhysicalId()))
            continue;
        desktopCamera = camera;
        break;
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

    QScopedPointer<QnVideowallSettingsDialog> dialog(new QnVideowallSettingsDialog(mainWindow()));
    dialog->loadFromResource(videowall);
    if (!dialog->exec())
        return;

    dialog->submitToResource(videowall);
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
        QUuid pcUuid = data.dataByRole[Qn::VideoWallPcGuidRole].value<QUuid>();
        if (pcUuid != pc.uuid)
            continue;

        itemsToDelete << workbenchItem;
    }

    foreach(QnWorkbenchItem *workbenchItem, itemsToDelete)
        layout->removeItem(workbenchItem);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    if (!videoWall->pcs()->hasItem(item.pcUuid))
        return;

    QnVideoWallPcData pc = videoWall->pcs()->getItem(item.pcUuid);
    QList<int> indices = getScreensByItem(calculateSnaps(pc.uuid, pc.screens), item.geometry);
    if (indices.isEmpty())
        return;

    if (indices.size() == 1) {
        // checking that required widget already exists - only for items, taking part of the screen
        foreach(QnWorkbenchItem *workbenchItem, layout->items()) {
            QnLayoutItemData data = workbenchItem->data();
            QUuid pcUuid = data.dataByRole[Qn::VideoWallPcGuidRole].value<QUuid>();
            if (pcUuid != item.pcUuid)
                continue;
            QList<int> screenIndices = data.dataByRole[Qn::VideoWallPcScreenIndicesRole].value<QList<int> >();
            if (screenIndices.contains(indices.first()))
                return; //widget exists and will handle event by itself
        }
    }

    addItemToLayout(layout->resource(), videoWall, pc, indices);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    //TODO: #GDM #VW implement screen size changes handling
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videoWall);
    if (!layout)
        return;

    if (!videoWall->pcs()->hasItem(item.pcUuid))
        return;

    QnVideoWallPcData pc = videoWall->pcs()->getItem(item.pcUuid);

    foreach(QnWorkbenchItem *workbenchItem, layout->items()) {
        QnLayoutItemData data = workbenchItem->data();
        QUuid pcUuid = data.dataByRole[Qn::VideoWallPcGuidRole].value<QUuid>();
        if (pcUuid != item.pcUuid)
            continue;

        QRectF combinedGeometry;
        QList<int> screenIndices = data.dataByRole[Qn::VideoWallPcScreenIndicesRole].value<QList<int> >();
        foreach(const QnVideoWallPcData::PcScreen &screen, pc.screens) {
            if (!screenIndices.contains(screen.index))
                continue;
            combinedGeometry = combinedGeometry.united(screen.desktopGeometry);
        }

        if (!combinedGeometry.contains(item.geometry))
            continue;

        // we found the widget containing removed item
        foreach (const QnVideoWallItem &existingItem, videoWall->items()->getItems()) {
            if (existingItem.pcUuid != item.pcUuid)
                continue;

            if (combinedGeometry.contains(existingItem.geometry))
                return; //that wasn't last item
        }

        layout->removeItem(workbenchItem);
        return; // no need to check other items
    }
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
    if (widget->resource()->flags() & QnResource::sync) {
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
    arguments << QLatin1String("--videowall");
    arguments << videowall->getId().toString();

    QUrl url = qnSettings->lastUsedConnection().url;
    url.setUserName(QString());
    url.setPassword(QString());

    arguments << QLatin1String("--auth");
    arguments << QLatin1String(url.toEncoded());

    return qnPlatform->shortcuts()->createShortcut(qApp->applicationFilePath(), destinationPath, videowall->getName(), arguments);
}

void QnWorkbenchVideoWallHandler::saveVideowall(const QnVideoWallResourcePtr& videowall) {
    connection2()->getVideowallManager()->save(videowall, this, [this, videowall] {
        if (!videowall->items()->getItems().isEmpty())
            return;
        QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videowall);
        if (layout && layout->resource())
            snapshotManager()->setFlags(layout->resource(), snapshotManager()->flags(layout->resource()) & ~Qn::ResourceIsChanged);
    });
}

void QnWorkbenchVideoWallHandler::saveVideowalls(const QSet<QnVideoWallResourcePtr> &videowalls) {
    foreach (const QnVideoWallResourcePtr &videowall, videowalls)
        saveVideowall(videowall);
}

bool QnWorkbenchVideoWallHandler::saveReviewLayout(const QnLayoutResourcePtr &layoutResource, std::function<void(int, ec2::ErrorCode)> callback) {
    QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(layoutResource);
    if (!layout) {
        return false;
    }

    QnVideoWallResourcePtr videowall = layoutResource->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();

    foreach(QnWorkbenchItem *item, layout->items()) {
        QnLayoutItemData data = item->data();
        QUuid pcUuid = data.dataByRole[Qn::VideoWallPcGuidRole].value<QUuid>();
        if (!videowall->pcs()->hasItem(pcUuid))
            continue;
        QnVideoWallPcData pc = videowall->pcs()->getItem(pcUuid);

        QList<int> screenIndices = data.dataByRole[Qn::VideoWallPcScreenIndicesRole].value<QList<int> >();
        if (screenIndices.size() < 1)
            continue;
        pc.screens[screenIndices.first()].layoutGeometry = data.combinedGeometry.toRect();
        videowall->pcs()->updateItem(pcUuid, pc);
    }

    connection2()->getVideowallManager()->save(videowall, this, 
        [this, callback]( int reqID, ec2::ErrorCode errorCode ) {
            callback(reqID, errorCode);
    } );

    return true;
}

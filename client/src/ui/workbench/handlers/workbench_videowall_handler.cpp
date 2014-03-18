#include "workbench_videowall_handler.h"

#include <QtCore/QProcess>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <boost/preprocessor/stringize.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <client/client_message_processor.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_pc_data.h>
#include <core/resource_management/resource_pool.h>

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include <recording/time_period.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/common/ui_resource_name.h>
#include <ui/dialogs/layout_name_dialog.h> //TODO: #GDM VW refactor
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/server_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>

#include <utils/color_space/image_correction.h>
#include <utils/common/checked_cast.h>
#include <utils/common/container.h>
#include <utils/common/json.h>
#include <utils/common/json_functions.h>
#include <utils/common/string.h>

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
        itemData.dataByRole[Qn::VideoWallPcGuidRole] = qVariantFromValue<QUuid>(pc.uuid);
        itemData.dataByRole[Qn::VideoWallPcScreenIndicesRole] = qVariantFromValue<QList<int> >(screens);
        layout->addItem(itemData);
    }

    const int identifyTimeout  = 5000;
    const int identifyFontSize = 100;

    const qreal defaultReviewAR = 1920.0 / 1080.0;
}

QnWorkbenchVideoWallHandler::QnWorkbenchVideoWallHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    m_videoWallMode.active = qnSettings->isVideoWallMode();
    m_videoWallMode.opening = false;
    m_videoWallMode.ready = false;

    m_controlMode.active = false;
    m_controlMode.sequence = 0;

    m_reviewMode.active = false;

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

        //TODO: #GDM VW may be we should override ::instance() ?
        QnClientMessageProcessor* clientMessageProcessor = static_cast<QnClientMessageProcessor*>(QnClientMessageProcessor::instance());

        connect(clientMessageProcessor,   &QnClientMessageProcessor::videoWallControlMessageReceived,
                this,                     &QnWorkbenchVideoWallHandler::at_eventManager_controlMessageReceived);

        //connect(clientMessageProcessor, SIGNAL(connectionClosed()) TODO: #GDM VW reinitialize window state if someone control us?
        connect(clientMessageProcessor,   &QnClientMessageProcessor::connectionOpened,  this,   &QnWorkbenchVideoWallHandler::at_connection_opened);
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
        connect(action(Qn::AddVideoWallItemsToUserAction),  &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_addVideoWallItemsToUserAction_triggered);
        connect(action(Qn::StartVideoWallControlAction),    &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered);
        connect(action(Qn::OpenVideoWallsReviewAction),     &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_openVideoWallsReviewAction_triggered);
        connect(action(Qn::SaveVideoWallReviewAction),      &QAction::triggered,        this,   &QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered);


        connect(display(),     &QnWorkbenchDisplay::widgetAdded,                        this,   &QnWorkbenchVideoWallHandler::at_display_widgetAdded);
        connect(display(),     &QnWorkbenchDisplay::widgetAboutToBeRemoved,             this,   &QnWorkbenchVideoWallHandler::at_display_widgetAboutToBeRemoved);

        connect(context(),     &QnWorkbenchContext::userChanged,                        this,   &QnWorkbenchVideoWallHandler::at_context_userChanged);

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

QnAppServerConnectionPtr QnWorkbenchVideoWallHandler::connection() const {
    return QnAppServerConnectionFactory::createConnection();
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

QRect QnWorkbenchVideoWallHandler::calculateSnapGeometry(const QList<QnVideoWallPcData::PcScreen> &localScreens) {
    QRect source = mainWindow()->geometry();

    if (localScreens.isEmpty())
        return source;

    ScreenSnaps localSnaps = calculateSnaps(qnSettings->pcUuid(), localScreens);

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
    //TODO: #GDM VW check edges validity
    //TODO: #GDM VW check that screens are not in use already

    return QRect(QPoint(left.value, top.value), QPoint(right.value, bottom.value));

}

void QnWorkbenchVideoWallHandler::attachLayout(const QnVideoWallResourcePtr &videoWall, const QnId &layoutId) {


    QDesktopWidget* desktop = qApp->desktop();
    QList<QnVideoWallPcData::PcScreen> localScreens;
    for (int i = 0; i < desktop->screenCount(); i++) {
        QnVideoWallPcData::PcScreen screen;
        screen.index = i;
        screen.desktopGeometry = desktop->screenGeometry(i);
        localScreens << screen;
    }

    QnVideoWallItem data;
    data.layout = layoutId;
    data.name = generateUniqueString([&videoWall] () {
        QStringList used;
        foreach (const QnVideoWallItem &item, videoWall->getItems())
            used << item.name;
        return used;
    }(), tr("Screen"), tr("Screen %1") );
    data.geometry = calculateSnapGeometry(localScreens);
    mainWindow()->setGeometry(data.geometry);   // WYSIWYG

    QUuid pcUuid = qnSettings->pcUuid();
    data.pcUuid = pcUuid;
    data.uuid = QUuid::createUuid();
    videoWall->addItem(data);

    QnVideoWallPcData pcData;
    pcData.uuid = pcUuid;
    pcData.screens = localScreens;

    if (!videoWall->hasPc(pcUuid))
        videoWall->addPc(pcData);
    else
        videoWall->updatePc(pcUuid, pcData);

    connection()->saveAsync(videoWall);
}

void QnWorkbenchVideoWallHandler::resetLayout(const QnVideoWallItemIndexList &items, const QnId &layoutId) {

    QList<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &item, items) {
        if (!item.videowall())
            continue;

        QnVideoWallItem existingItem = item.videowall()->getItem(item.uuid());
        existingItem.layout = layoutId;
        item.videowall()->updateItem(item.uuid(), existingItem);

        if (!videoWalls.contains(item.videowall()))
            videoWalls << item.videowall();

    }

    foreach (const QnVideoWallResourcePtr& videoWall, videoWalls)
        connection()->saveAsync(videoWall);
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
    //--videowall {1f91dc77-229a-4b1e-9b07-99a5c2650a5a} --auth https://127.0.0.1:7011
#endif
    QProcess::startDetached(qApp->applicationFilePath(), arguments);
}

void QnWorkbenchVideoWallHandler::openVideoWallItem(const QnVideoWallResourcePtr &videoWall) {
    if (!videoWall) {
        qWarning() << "Warning: videowall not exists anymore, cannot open videowall item";
        closeInstance();
        return;
    }

    workbench()->clear();

    QnVideoWallItem item = videoWall->getItem(m_videoWallMode.instanceGuid);
    mainWindow()->setGeometry(item.geometry);

    QDesktopWidget* desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); i++) {
        QRect screen = desktop->screenGeometry(i);
        if (item.geometry == screen)
            menu()->action(Qn::EffectiveMaximizeAction)->trigger();
    }

    QnLayoutResourcePtr layout = qnResPool->getResourceById(item.layout, QnResourcePool::AllResources).dynamicCast<QnLayoutResource>();
    if (layout)
        menu()->trigger(Qn::OpenSingleLayoutAction, layout);
    sendInstanceGuid();
}

void QnWorkbenchVideoWallHandler::closeInstance() {
    QTimer::singleShot(10, action(Qn::ExitAction), SLOT(trigger()));
}

void QnWorkbenchVideoWallHandler::sendInstanceGuid() {
    connection()->sendVideoWallInstanceId(m_videoWallMode.instanceGuid);
}

void QnWorkbenchVideoWallHandler::sendMessage(QnVideoWallControlMessage message) {
    Q_ASSERT(m_controlMode.active);

    message[sequenceKey] = QString::number(m_controlMode.sequence++);
    message[pcUuidKey] = m_controlMode.pcUuid;
#ifdef SENDER_DEBUG
    qDebug() << "SENDER: sending message" << message;
#endif
    foreach (QnVideoWallItemIndex index, targetList()) {
        message.videoWallGuid = index.videowall()->getGuid();
        message.instanceGuid = index.uuid();
        connection()->sendVideoWallControlMessage(message);
    }
}

void QnWorkbenchVideoWallHandler::handleMessage(const QnVideoWallControlMessage &message, const QUuid &controllerUuid, qint64 sequence) {
#ifdef RECEIVER_DEBUG
    qDebug() << "RECEIVER: handling message" << message;
#endif

    if (sequence >= 0)
        m_videoWallMode.sequenceByPcUuid[controllerUuid] = sequence;

    switch (static_cast<QnVideoWallControlMessage::QnVideoWallControlOperation>(message.operation)) {
    case QnVideoWallControlMessage::Exit:
    {
        closeInstance();
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
        QUuid uuid(message[uuidKey]);
        QnMediaResourceWidget* widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(uuid));
        if (widget)
            widget->setMotionSelection(regions);
        break;
    }
    case QnVideoWallControlMessage::Identify:
    {
        QnVideoWallResourcePtr videoWall = qnResPool->getResourceByGuid(m_videoWallMode.guid).dynamicCast<QnVideoWallResource>();
        if (!videoWall)
            return;

        QnVideoWallItem data = videoWall->getItem(m_videoWallMode.instanceGuid);
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
    }
}

void QnWorkbenchVideoWallHandler::setReviewMode(bool active) {
    if (m_reviewMode.active == active)
        return;
    m_reviewMode.active = active;
    //TODO: #GDM VW what should we update or notify? Is it still required?
}


void QnWorkbenchVideoWallHandler::updateMode() {
    QnWorkbenchLayout* layout = workbench()->currentLayout();

    QUuid itemUuid = layout->data(Qn::VideoWallItemGuidRole).value<QUuid>();
    bool control = !itemUuid.isNull();
    setControlMode(control);

    QnVideoWallResourcePtr videoWall = layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    bool review = !videoWall.isNull();
    setReviewMode(review);
}

void QnWorkbenchVideoWallHandler::submitDelayedItemOpen() {

    // not logged in yet
    if (!m_videoWallMode.ready)
        return;

    // already opened or not submitted yet
    if (!m_videoWallMode.opening)
        return;

    m_videoWallMode.opening = false;

    QnVideoWallResourcePtr videoWall = qnResPool->getResourceByGuid(m_videoWallMode.guid).dynamicCast<QnVideoWallResource>();
    if (!videoWall || videoWall->getItems().isEmpty()) {
        if (!videoWall)
            qWarning() << "Warning: videowall not exists, cannot start videowall on this pc";
        else
            qWarning() << "Warning: videowall is empty, cannot start videowall on this pc";
        closeInstance();
        return;
    }

    QUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull()) {
        qWarning() << "Warning: pc UUID is null, cannot start videowall on this pc";
        closeInstance();
        return;
    }

    connect(videoWall, &QnVideoWallResource::itemChanged, this, &QnWorkbenchVideoWallHandler::at_videoWall_itemChanged_activeMode);
    connect(videoWall, &QnVideoWallResource::itemRemoved, this, &QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode);

    bool master = m_videoWallMode.instanceGuid.isNull();
    if (master) {
        bool first = true;

        foreach (const QnVideoWallItem &item, videoWall->getItems()) {
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

        foreach(const QnVideoWallItem &item, videoWall->getItems()) {
            if (item.layout == currentId)
                indexes << QnVideoWallItemIndex(videoWall, item.uuid);
        }
    }

    return indexes;
}

QnWorkbenchLayout* QnWorkbenchVideoWallHandler::findReviewModeLayout(const QnVideoWallResourcePtr &videoWall) const {
    foreach (const QnLayoutResourcePtr &layout, resourcePool()->getResources().filtered<QnLayoutResource>()) {
        if (layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>() == videoWall)
            return QnWorkbenchLayout::instance(layout);
    }
    return NULL;
}

/*------------------------------------ HANDLERS ------------------------------------------*/

void QnWorkbenchVideoWallHandler::at_connection_opened() {
    if (!m_videoWallMode.instanceGuid.isNull())
        sendInstanceGuid();
}

void QnWorkbenchVideoWallHandler::at_context_userChanged() {
    m_attaching.clear();
    m_resetting.clear();
    m_savingReviews.clear();
}

void QnWorkbenchVideoWallHandler::at_newVideoWallAction_triggered() {
    //TODO: #GDM VW refactor to corresponding dialog
    QScopedPointer<QnLayoutNameDialog> dialog(new QnLayoutNameDialog(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, mainWindow()));
    dialog->setWindowTitle(tr("New Video Wall"));
    dialog->setText(tr("Enter the name of the video wall to create:"));
    dialog->setName(
        generateUniqueString([] () {
                QStringList used;
                foreach(const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::videowall))
                    used << resource->getName();
                return used;
            }(), tr("Videowall"), tr("Videowall %1") )
    );
    dialog->setWindowModality(Qt::ApplicationModal);

    if(!dialog->exec())
        return;

    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setGuid(QUuid::createUuid().toString());
    videoWall->setName(dialog->name());
    videoWall->setParentId(0);
    connection()->saveAsync(videoWall); //TODO: #GDM VW show message if not successfull
}

void QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered() {
    if (!context()->user())
        return;

    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if(videoWall.isNull())
        return;    // TODO: #GDM VW implement videoWall selection / creation dialog

    // TODO: #GDM VW ugly copypaste, make common check in action_conditions
    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (snapshotManager()->isFile(layout))
        return; // TODO: #GDM VW action should not be available

    // attaching review layouts is disallowed
    if (layout->data().contains(Qn::VideoWallResourceRole))
        return;  // TODO: #GDM VW action should not be available

    //TODO: #GDM VW extract copy layout method
    QnLayoutResourcePtr newLayout(new QnLayoutResource());
    newLayout->setGuid(QUuid::createUuid().toString());
    newLayout->setName(generateUniqueLayoutName(context()->user(), tr("VideoWall Layout"),  tr("VideoWall Layout %1")));
    newLayout->setParentId(context()->user()->getId());
    newLayout->setCellSpacing(QSizeF(0.0, 0.0));
    newLayout->setCellAspectRatio(layout->cellAspectRatio());
    newLayout->setBackgroundImageFilename(layout->backgroundImageFilename());
    newLayout->setBackgroundOpacity(layout->backgroundOpacity());
    newLayout->setBackgroundSize(layout->backgroundSize());
    newLayout->setUserCanEdit(true);
    resourcePool()->addResource(newLayout);

    QnLayoutItemDataList items = layout->getItems().values();
    QHash<QUuid, QUuid> newUuidByOldUuid;
    for(int i = 0; i < items.size(); i++) {
        QUuid newUuid = QUuid::createUuid();
        newUuidByOldUuid[items[i].uuid] = newUuid;
        items[i].uuid = newUuid;
    }
    for(int i = 0; i < items.size(); i++)
        items[i].zoomTargetUuid = newUuidByOldUuid.value(items[i].zoomTargetUuid, QUuid());
    newLayout->setItems(items);

    int requestId = snapshotManager()->save(newLayout, this, SLOT(at_videoWall_layout_saved(int, const QnResourceList &, int)));
    m_attaching.insert(requestId, videoWall);
}

void QnWorkbenchVideoWallHandler::at_detachFromVideoWallAction_triggered() {
    QnVideoWallItemIndexList items = menu()->currentParameters(sender()).videoWallItems();

    QList<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &item, items) {
        if (!item.videowall())
            continue;

        QnVideoWallItem existingItem = item.videowall()->getItem(item.uuid());
        existingItem.layout = QnId();
        item.videowall()->updateItem(item.uuid(), existingItem);

        if (!videoWalls.contains(item.videowall()))
            videoWalls << item.videowall();
    }

    foreach (const QnVideoWallResourcePtr& videoWall, videoWalls)
        connection()->saveAsync(videoWall);
}

void QnWorkbenchVideoWallHandler::at_resetVideoWallLayoutAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList items = parameters.videoWallItems();
    QnLayoutResourcePtr layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole,
                                                                          workbench()->currentLayout()->resource());

    // TODO: #GDM VW ugly copypaste, make common check in action_conditions
    if (snapshotManager()->isFile(layout))
        return; // TODO: #GDM VW action should not be available

    // attaching review layouts is disallowed
    if (layout->data().contains(Qn::VideoWallResourceRole))
        return;  // TODO: #GDM VW action should not be available

    layout->setCellSpacing(QSizeF(0.0, 0.0));
    layout->setUserCanEdit(true);

    if (snapshotManager()->isLocal(layout) || snapshotManager()->isModified(layout)) {
        int requestId = snapshotManager()->save(workbench()->currentLayout()->resource(), this, SLOT(at_videoWall_layout_saved(int, const QnResourceList &, int)));
        m_resetting.insert(requestId, items);
        return;
    }

    resetLayout(items, layout->getId());

}

void QnWorkbenchVideoWallHandler::at_deleteVideoWallItemAction_triggered() {
    QnVideoWallItemIndexList items = menu()->currentParameters(sender()).videoWallItems();

    QList<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &item, items) {
        if (!item.videowall())
            continue;
        item.videowall()->removeItem(item.uuid());
        if (!videoWalls.contains(item.videowall()))
            videoWalls << item.videowall();

    }

    foreach (const QnVideoWallResourcePtr& videoWall, videoWalls)
        connection()->saveAsync(videoWall);
}

void QnWorkbenchVideoWallHandler::at_startVideoWallAction_triggered() {
    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if(videoWall.isNull())
        return;

    QUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull()) {
        qWarning() << "Warning: pc UUID is null, cannot start videowall on this pc";
        closeInstance();
        return;
    }

    bool itemFound = false;
    foreach (const QnVideoWallItem &item, videoWall->getItems()) {
        if (item.pcUuid != pcUuid)
            continue;
        itemFound = true;
        break;
    }
    if (!itemFound) {
        qWarning() << "Warning: no items for this pc, cannot start videowall";
        return;
    }


    QStringList arguments;
    arguments << QLatin1String("--videowall");
    arguments << videoWall->getGuid();
    openNewWindow(arguments);
}

void QnWorkbenchVideoWallHandler::at_stopVideoWallAction_triggered() {
    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if(videoWall.isNull())
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::Exit);
    message.videoWallGuid = videoWall->getGuid();
    foreach (const QnVideoWallItem &item, videoWall->getItems()) {
        message.instanceGuid = item.uuid;
        connection()->sendVideoWallControlMessage(message);
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
    QnVideoWallItemIndexList items = parameters.videoWallItems();
    if (items.isEmpty())
        return;

    QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();


    QList<QnVideoWallResourcePtr> videoWalls;

    foreach (const QnVideoWallItemIndex &item, items) {
        if (!item.videowall())
            continue;

        QnVideoWallItem existingItem = item.videowall()->getItem(item.uuid());
        existingItem.name = name;
        item.videowall()->updateItem(item.uuid(), existingItem);

        if (!videoWalls.contains(item.videowall()))
            videoWalls << item.videowall();

    }

    foreach (const QnVideoWallResourcePtr& videoWall, videoWalls)
        connection()->saveAsync(videoWall);
}

void QnWorkbenchVideoWallHandler::at_identifyVideoWallAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList items = parameters.videoWallItems();
    if (items.isEmpty()) {
        foreach (QnVideoWallResourcePtr videoWall, parameters.resources().filtered<QnVideoWallResource>()) {
            if(!videoWall)
                continue;
            foreach (const QnVideoWallItem &item, videoWall->getItems()) {
                items << QnVideoWallItemIndex(videoWall, item.uuid);
            }
        }
    }

    QnVideoWallControlMessage message(QnVideoWallControlMessage::Identify);
    foreach (const QnVideoWallItemIndex &item, items) {
        message.videoWallGuid = item.videowall()->getGuid();
        message.instanceGuid = item.uuid();
        connection()->sendVideoWallControlMessage(message);
    }
}

void QnWorkbenchVideoWallHandler::at_addVideoWallItemsToUserAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnVideoWallItemIndexList items = parameters.videoWallItems();
    QnUserResourcePtr user = parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole);

    if (!user || items.isEmpty())
        return;

    foreach (QnVideoWallItemIndex index, items)
        user->addVideoWallItem(index.uuid());
    connection()->saveAsync(user);
}

void QnWorkbenchVideoWallHandler::at_startVideoWallControlAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnWorkbenchLayout *layout = NULL;

    QnVideoWallItemIndexList items = parameters.videoWallItems();
    foreach (QnVideoWallItemIndex index, items) {
        if (!index.videowall())
            continue;
        QnVideoWallItem item = index.videowall()->getItem(index.uuid());
        if (!item.layout.isValid())
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

        QnWorkbenchLayout* existingLayout = findReviewModeLayout(videoWall);
        if (existingLayout) {
            workbench()->setCurrentLayout(existingLayout);
            return;
        }

        /* Construct and add a new layout. */
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setGuid(QUuid::createUuid().toString());
        layout->setName(videoWall->getName());
        if(context()->user())
            layout->setParentId(context()->user()->getId());

        layout->setCellSpacing(0.0, 0.0);
        layout->setCellAspectRatio(defaultReviewAR);
        layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission));
        layout->setData(Qn::VideoWallResourceRole, qVariantFromValue(videoWall));

        foreach (const QnVideoWallPcData &pc, videoWall->getPcs()) {

            QSet<int> usedScreens;

            foreach (const QnVideoWallItem &item, videoWall->getItems()) {
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

//                int idx = screens.first();
//                int firstScreen = qnIndexOf(pc.screens, [idx](const QnVideoWallPcData::PcScreen &screen) {return screen.index == idx;});
//                if (firstScreen < 0)
//                    continue;

//                QnLayoutItemData itemData;
//                itemData.uuid = QUuid::createUuid();
//                itemData.combinedGeometry = pc.screens[firstScreen].layoutGeometry;
//                if (itemData.combinedGeometry.isValid())
//                    itemData.flags = Qn::Pinned;
//                else
//                    itemData.flags = Qn::PendingGeometryAdjustment;
//                itemData.resource.id = videoWall->getId();
//                itemData.dataByRole[Qn::VideoWallPcGuidRole] = qVariantFromValue<QUuid>(pc.uuid);
//                itemData.dataByRole[Qn::VideoWallPcScreenIndicesRole] = qVariantFromValue<QList<int> >(screens);
//                layout->addItem(itemData);
            }


        }

        resourcePool()->addResource(layout);
        menu()->trigger(Qn::OpenSingleLayoutAction, layout);
    }
}

void QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered() {
    QnWorkbenchLayout* layout = workbench()->currentLayout();
    QnVideoWallResourcePtr videowall =  layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!videowall)
        return;

    foreach(QnWorkbenchItem *item, layout->items()) {
        QnLayoutItemData data = item->data();
        QUuid pcUuid = data.dataByRole[Qn::VideoWallPcGuidRole].value<QUuid>();
        if (!videowall->hasPc(pcUuid))
            continue;
        QnVideoWallPcData pc = videowall->getPc(pcUuid);

        QList<int> screenIndices = data.dataByRole[Qn::VideoWallPcScreenIndicesRole].value<QList<int> >();
        if (screenIndices.size() < 1)
            continue;
        pc.screens[screenIndices.first()].layoutGeometry = data.combinedGeometry.toRect();
        videowall->updatePc(pcUuid, pc);
    }

    snapshotManager()->setFlags(layout->resource(), snapshotManager()->flags(layout->resource()) | Qn::ResourceIsBeingSaved);
    int handle = connection()->saveAsync(videowall, this, SLOT(at_videoWall_saved(int,QnResourceList,int)));
    m_savingReviews[handle] = layout->resource();

}

void QnWorkbenchVideoWallHandler::at_videoWall_saved(int status, const QnResourceList &resources, int handle) {
    Q_UNUSED(resources)
    if (!m_savingReviews.contains(handle))
        return;

    QnLayoutResourcePtr layout = m_savingReviews.take(handle);
    snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsBeingSaved);

    if (status == 0)
        snapshotManager()->setFlags(layout, snapshotManager()->flags(layout) & ~Qn::ResourceIsChanged);
}

void QnWorkbenchVideoWallHandler::at_videoWall_layout_saved(int status, const QnResourceList &resources, int handle) {
    if (status != 0 || resources.size() == 0) {
        m_attaching.remove(handle);
        m_resetting.remove(handle);
        //TODO: #GDM VW should we display an error message?
        return;
    }

    QnLayoutResourcePtr layout = resources.first().dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    if (m_attaching.contains(handle)) {
        QnVideoWallResourcePtr videoWall = m_attaching.take(handle);
        if (!videoWall)
            return;
        attachLayout(videoWall, layout->getId());
    } else if (m_resetting.contains(handle)) {
        QnVideoWallItemIndexList items = m_resetting.take(handle);
        resetLayout(items, layout->getId());
    }
}

void QnWorkbenchVideoWallHandler::at_resPool_resourceAdded(const QnResourcePtr &resource) {
    if (m_videoWallMode.active) {
        if (resource->getGuid() != m_videoWallMode.guid.toString())
            return;

        if (m_videoWallMode.ready)
            return;
        m_videoWallMode.ready = true;
        submitDelayedItemOpen();
    } else {
        if (QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>()) {
            connect(videoWall, &QnVideoWallResource::pcAdded,       this, &QnWorkbenchVideoWallHandler::at_videoWall_pcAdded);
            connect(videoWall, &QnVideoWallResource::pcChanged,     this, &QnWorkbenchVideoWallHandler::at_videoWall_pcChanged);
            connect(videoWall, &QnVideoWallResource::pcRemoved,     this, &QnWorkbenchVideoWallHandler::at_videoWall_pcRemoved);
            connect(videoWall, &QnVideoWallResource::itemAdded,     this, &QnWorkbenchVideoWallHandler::at_videoWall_itemAdded);
            connect(videoWall, &QnVideoWallResource::itemChanged,   this, &QnWorkbenchVideoWallHandler::at_videoWall_itemChanged);
            connect(videoWall, &QnVideoWallResource::itemRemoved,   this, &QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved);
        }
    }
}

void QnWorkbenchVideoWallHandler::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    if (m_videoWallMode.active) {
        if (resource->getGuid() != m_videoWallMode.guid.toString())
            return;
        closeInstance();
    } else {
        if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            int key = m_savingReviews.key(layout, -1);
            while (key > 0) {
                m_savingReviews.remove(key);
                key = m_savingReviews.key(layout, -1);
            }
        }

        if (QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>()) {
            disconnect(videoWall, NULL, this, NULL);
            int key = m_attaching.key(videoWall, -1);
            while (key > 0) {
                m_attaching.remove(key);
                key = m_attaching.key(videoWall, -1);
            }
        }

    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
    QnWorkbenchLayout* layout = findReviewModeLayout(videoWall);
    if (!layout)
        return;

    foreach (const QnVideoWallItem &item, videoWall->getItems()) {
        if (item.pcUuid != pc.uuid)
            continue;
        at_videoWall_itemAdded(videoWall, item);
    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
    //TODO: #GDM VW implement screen size changes handling
}

void QnWorkbenchVideoWallHandler::at_videoWall_pcRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc) {
    QnWorkbenchLayout* layout = findReviewModeLayout(videoWall);
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
    QnWorkbenchLayout* layout = findReviewModeLayout(videoWall);
    if (!layout)
        return;

    if (!videoWall->hasPc(item.pcUuid))
        return;

    QnVideoWallPcData pc = videoWall->getPc(item.pcUuid);
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
    //TODO: #GDM VW implement screen size changes handling
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnWorkbenchLayout* layout = findReviewModeLayout(videoWall);
    if (!layout)
        return;

    if (!videoWall->hasPc(item.pcUuid))
        return;

    QnVideoWallPcData pc = videoWall->getPc(item.pcUuid);

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
        foreach (const QnVideoWallItem &existingItem, videoWall->getItems()) {
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
    if (videoWall->getGuid() != m_videoWallMode.guid.toString() || item.uuid != m_videoWallMode.instanceGuid)
        return;
    openVideoWallItem(videoWall);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_activeMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    if (videoWall->getGuid() != m_videoWallMode.guid.toString() || item.uuid != m_videoWallMode.instanceGuid)
        return;
    closeInstance();
}

void QnWorkbenchVideoWallHandler::at_eventManager_controlMessageReceived(const QnVideoWallControlMessage &message) {
    if (message.instanceGuid != m_videoWallMode.instanceGuid)
        return;

    // Unorderer broadcast messages such as Exit or Identify
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
    //TODO: #GDM VW what if one message is lost forever? timeout?
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
    sendMessage(message);
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

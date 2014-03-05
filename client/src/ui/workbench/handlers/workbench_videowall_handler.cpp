#include "workbench_videowall_handler.h"

#include <QtCore/QProcess>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <boost/preprocessor/stringize.hpp>

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
#include <ui/graphics/items/standard/graphics_message_box.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
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
        case Qn::ItemFrameColorRole:
            return "ItemFrameColorRole";
        case Qn::ItemSliderWindowRole:
            return "ItemSliderWindowRole";
        case Qn::ItemSliderSelectionRole:
            return "ItemSliderSelectionRole";
        case Qn::ItemImageEnhancementRole:
            return "ItemImageEnhancementRole";
        case Qn::ItemImageDewarpingRole:
            return "ItemImageDewarpingRole";
        default:
            return "unknown role " + QString::number(role).toUtf8();
        }
    }
#endif
}

QnWorkbenchVideoWallHandler::QnWorkbenchVideoWallHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    m_videoWallMode.active = qnSettings->isVideoWallMode();
    m_videoWallMode.opening = false;
    m_videoWallMode.ready = false;

    m_controlMode.active = false;
    m_controlMode.sequence = 0;

    m_reviewMode.active = false;
    m_reviewMode.inGeometryChange = false;

    QUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull()) {
        pcUuid = QUuid::createUuid();
        qnSettings->setPcUuid(pcUuid);
    }
    m_controlMode.pcUuid = pcUuid.toString();

    m_screenSnaps.calculated = false;

    /* Common connections */
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceRemoved(const QnResourcePtr &)));

    if (m_videoWallMode.active) {

        /* Videowall reaction actions */

        connect(action(Qn::DelayedOpenVideoWallItemAction),     SIGNAL(triggered()),        this,   SLOT(at_delayedOpenVideoWallItemAction_triggered()));

        connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resPool_resourceAdded(const QnResourcePtr &)));
        foreach(const QnResourcePtr &resource, resourcePool()->getResources())
            at_resPool_resourceAdded(resource);

        connect(QnClientMessageProcessor::instance(),  SIGNAL(videoWallControlMessageReceived(QnVideoWallControlMessage)),
                this, SLOT(at_eventManager_controlMessageReceived(const QnVideoWallControlMessage &)));

        //connect(QnClientMessageProcessor::instance(), SIGNAL(connectionClosed()) TODO: #GDM VW reinitialize window state if someone control us?
        connect(QnClientMessageProcessor::instance(),       SIGNAL(connectionOpened()), this,   SLOT(at_connection_opened()));
    } else {

        /* Control videowall actions */

        connect(action(Qn::NewVideoWallAction),             SIGNAL(triggered()),        this,   SLOT(at_newVideoWallAction_triggered()));
        connect(action(Qn::AttachToVideoWallAction),        SIGNAL(triggered()),        this,   SLOT(at_attachToVideoWallAction_triggered()));
        connect(action(Qn::DetachFromVideoWallAction),      SIGNAL(triggered()),        this,   SLOT(at_detachFromVideoWallAction_triggered()));
        connect(action(Qn::ResetVideoWallLayoutAction),     SIGNAL(triggered()),        this,   SLOT(at_resetVideoWallLayoutAction_triggered()));
        connect(action(Qn::DeleteVideoWallItemAction),      SIGNAL(triggered()),        this,   SLOT(at_deleteVideoWallItemAction_triggered()));
        connect(action(Qn::StartVideoWallAction),           SIGNAL(triggered()),        this,   SLOT(at_startVideoWallAction_triggered()));
        connect(action(Qn::StopVideoWallAction),            SIGNAL(triggered()),        this,   SLOT(at_stopVideoWallAction_triggered()));
        connect(action(Qn::RenameAction),                   SIGNAL(triggered()),        this,   SLOT(at_renameAction_triggered()));
        connect(action(Qn::IdentifyVideoWallAction),        SIGNAL(triggered()),        this,   SLOT(at_identifyVideoWallAction_triggered()));
        connect(action(Qn::AddVideoWallItemsToUserAction),  SIGNAL(triggered()),        this,   SLOT(at_addVideoWallItemsToUserAction_triggered()));
        connect(action(Qn::StartVideoWallControlAction),    SIGNAL(triggered()),        this,   SLOT(at_startVideoWallControlAction_triggered()));
        connect(action(Qn::OpenVideoWallsReviewAction),     SIGNAL(triggered()),        this,   SLOT(at_openVideoWallsReviewAction_triggered()));
        connect(action(Qn::SaveVideoWallReviewAction),      SIGNAL(triggered()),        this,   SLOT(at_saveVideoWallReviewAction_triggered()));


        connect(display(),          SIGNAL(widgetAdded(QnResourceWidget*)),             this,   SLOT(at_display_widgetAdded(QnResourceWidget*)));
        connect(display(),          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget*)),  this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget*)));

        connect(context(),          SIGNAL(userChanged(const QnUserResourcePtr &)),     this,   SLOT(at_context_userChanged()));

        connect(workbench(),        SIGNAL(currentLayoutAboutToBeChanged()),            this,   SLOT(at_workbench_currentLayoutAboutToBeChanged()));
        connect(workbench(),        SIGNAL(currentLayoutChanged()),                     this,   SLOT(at_workbench_currentLayoutChanged()));

        connect(navigator(),        SIGNAL(positionChanged()),                          this,   SLOT(at_navigator_positionChanged()));
        connect(navigator(),        SIGNAL(speedChanged()),                             this,   SLOT(at_navigator_speedChanged()));

        connect(context()->instance<QnWorkbenchStreamSynchronizer>(), SIGNAL(runningChanged()), this, SLOT(at_workbenchStreamSynchronizer_runningChanged()));

        foreach(QnResourceWidget *widget, display()->widgets())
            at_display_widgetAdded(widget);
    }


}

QnWorkbenchVideoWallHandler::~QnWorkbenchVideoWallHandler() {

}

QnAppServerConnectionPtr QnWorkbenchVideoWallHandler::connection() const {
    return QnAppServerConnectionFactory::createConnection();
}

int findNearest(QList<int> list, int value) {

    QList<int>::ConstIterator i = qLowerBound(list.constBegin(), list.constEnd(), value);
    if (*i == value)
        return value;
    int idx = (i - list.constBegin());
    if (idx <= 0)
        return list.first();
    if (idx >= list.size())
        return list.last();

    int prev = list[idx - 1];
    int next = list[idx];
    if (qAbs(value - prev) <= qAbs(value - next))
        return prev;
    return next;
}

QRect QnWorkbenchVideoWallHandler::calculateSnapGeometry() {
    if (!m_screenSnaps.calculated) {

        QDesktopWidget* desktop = qApp->desktop();
        if (desktop->screenCount() == 0)
            return mainWindow()->geometry();

        QSet<int> leftSet, rightSet, topSet, bottomSet;

        for (int i = 0; i < desktop->screenCount(); i++) {
            QRect screen = desktop->screenGeometry(i);
            int w = screen.width() / 2;
            int h = screen.height() / 2;

            leftSet << screen.left() << screen.left() + w + 1;
            rightSet << screen.left() + w << screen.right();
            topSet << screen.top() << screen.top() + h + 1;
            bottomSet << screen.top() + h << screen.bottom();
        }

        QList<int> left = leftSet.values();
        qSort(left);
        m_screenSnaps.left = left;

        QList<int> right = rightSet.values();
        qSort(right);
        m_screenSnaps.right = right;

        QList<int> top = topSet.values();
        qSort(top);
        m_screenSnaps.top = top;

        QList<int> bottom = bottomSet.values();
        qSort(bottom);
        m_screenSnaps.bottom = bottom;

        m_screenSnaps.calculated = true;
    }

    QRect source = mainWindow()->geometry();
    QPoint topLeft(
                findNearest(m_screenSnaps.left, source.left()),
                findNearest(m_screenSnaps.top, source.top()));
    QPoint bottomRight(
                findNearest(m_screenSnaps.right, source.right()),
                findNearest(m_screenSnaps.bottom, source.bottom()));
    return QRect(topLeft, bottomRight);
}

void QnWorkbenchVideoWallHandler::attachLayout(const QnVideoWallResourcePtr &videoWall, const QnId &layoutId) {
    QnVideoWallItem data;
    data.layout = layoutId;
    data.name = generateUniqueString([&videoWall] () {
        QStringList used;
        foreach (const QnVideoWallItem &item, videoWall->getItems())
            used << item.name;
        return used;
    }(), tr("Screen"), tr("Screen %1") );
    data.geometry = calculateSnapGeometry();
    mainWindow()->setGeometry(data.geometry);   // WYSIWYG

    QUuid pcUuid = qnSettings->pcUuid();
    data.pcUuid = pcUuid;
    data.uuid = QUuid::createUuid();
    videoWall->addItem(data);

    //TODO: #GDM VW when the screen configuration should be updated?
    if (videoWall->getPc(pcUuid).screens.isEmpty()) {
        QnVideoWallPcData pcData;
        pcData.uuid = pcUuid;

        QDesktopWidget* desktop = qApp->desktop();
        for (int i = 0; i < desktop->screenCount(); i++) {
            QnVideoWallPcData::PcScreen screen;
            screen.index = i;
            screen.geometry = desktop->screenGeometry(i);
            pcData.screens.append(screen);
        }

        videoWall->addPc(pcData);
    }

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

//TODO: #GDM VW use action
void QnWorkbenchVideoWallHandler::openNewWindow(const QStringList &args) {
    QStringList arguments = args;

    QUrl url = qnSettings->lastUsedConnection().url;
    url.setUserName(QString());
    url.setPassword(QString());

    arguments << QLatin1String("--auth");
    arguments << QLatin1String(url.toEncoded());

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
        QnWorkbenchItem* item = new QnWorkbenchItem(resourceUid, uuid, workbench()->currentLayout());
        item->setGeometry(geometry);
        item->setZoomRect(zoomRect);
        item->setRotation(rotation);
        item->setFlag(Qn::PendingGeometryAdjustment);
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
        case Qn::ItemFrameColorRole:
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
            QnGraphicsMessageBox::identify(data.name);
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

void QnWorkbenchVideoWallHandler::addScreenToReviewLayout(const QnVideoWallItem &source, const QnLayoutResourcePtr &layout, QRect &boundingRect, QRect geometry) {
    if (!source.layout.isValid())
        return;

    QnVideoWallResourcePtr videoWall = layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!videoWall)
        return;

    QnVideoWallPcData pc = videoWall->getPc(source.pcUuid);
    if (pc.screens.isEmpty())
        return;

    QRect united = pc.unitedGeometry();

    QSet<int> leftSet, topSet;
    foreach(QnVideoWallPcData::PcScreen screen, pc.screens) {
        int w = screen.geometry.width() / 2;
        int h = screen.geometry.height() / 2;
        leftSet << screen.geometry.left() << screen.geometry.left() + w + 1;
        topSet << screen.geometry.top() << screen.geometry.top() + h + 1;
    }

    int xsnaps = leftSet.size();
    int ysnaps = topSet.size();

    qreal xstep = (qreal)united.width() / xsnaps;
    qreal ystep = (qreal)united.height() / ysnaps;
    if (pc.geometry.width() > 0 && pc.geometry.height() > 0) {
        xstep /= pc.geometry.width();
        ystep /= pc.geometry.height();
    }

    int x = qRound(qreal(source.geometry.left() - united.left()) / xstep);
    int y = qRound(qreal(source.geometry.top() - united.top()) / ystep);

    if (!geometry.isValid()) {
        int w = qRound(qreal(source.geometry.width()) / xstep);
        int h = qRound(qreal(source.geometry.height()) / ystep);

        if (!pc.geometry.isValid()) {
            if (boundingRect.isValid()) {
                if (boundingRect.width() > boundingRect.height()) {
                    pc.geometry.setLeft(boundingRect.left());
                    pc.geometry.setTop(boundingRect.top() + boundingRect.height() + 1);
                } else {
                    pc.geometry.setLeft(boundingRect.left() + boundingRect.width() + 1);
                    pc.geometry.setTop(boundingRect.top());
                }
            }
            pc.geometry.setWidth(1);
            pc.geometry.setHeight(1);
            videoWall->updatePc(pc.uuid, pc);
        }
        boundingRect = boundingRect.united(QRect(pc.geometry.left(), pc.geometry.top(), united.width() / xstep, united.height() / ystep));

        geometry = QRect(x, y, w, h);
        geometry.adjust(pc.geometry.left(), pc.geometry.top(), pc.geometry.left(), pc.geometry.top());
    }

    QnLayoutItemData item;
    item.flags = Qn::Pinned;
    item.uuid = QUuid::createUuid();
    item.combinedGeometry = geometry;
    item.resource.id = source.layout;
    item.dataByRole[Qn::VideoWallResourceRole] = qVariantFromValue(videoWall);
    item.dataByRole[Qn::VideoWallItemGuidRole] = qVariantFromValue(source.uuid);
    item.dataByRole[Qn::ItemScreenPositionRole] = qVariantFromValue(QPoint(x, y));

    layout->addItem(item);
    layout->setCellAspectRatio(xstep / ystep);
}

void QnWorkbenchVideoWallHandler::setControlMode(bool active) {
    if (m_controlMode.active == active)
        return;

    QnWorkbenchLayout* layout = workbench()->currentLayout();
    if (active) {
        connect(workbench(),    SIGNAL(itemChanged(Qn::ItemRole)),         this,    SLOT(at_workbench_itemChanged(Qn::ItemRole)));
        connect(layout,         SIGNAL(itemAdded(QnWorkbenchItem *)),      this,    SLOT(at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *)));
        connect(layout,         SIGNAL(itemRemoved(QnWorkbenchItem *)),    this,    SLOT(at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem *)));
        connect(layout,         SIGNAL(zoomLinkAdded(QnWorkbenchItem*,QnWorkbenchItem*)),   this, SLOT(at_workbenchLayout_zoomLinkAdded(QnWorkbenchItem*,QnWorkbenchItem*)));
        connect(layout,         SIGNAL(zoomLinkRemoved(QnWorkbenchItem*,QnWorkbenchItem*)), this, SLOT(at_workbenchLayout_zoomLinkRemoved(QnWorkbenchItem*,QnWorkbenchItem*)));

        foreach (QnWorkbenchItem* item, layout->items()) {
             connect(item,      SIGNAL(dataChanged(int)),                  this,    SLOT(at_workbenchLayoutItem_dataChanged(int)));
        }

        m_controlMode.active = active;
        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStarted));
    } else {
        disconnect(workbench(), SIGNAL(itemChanged(Qn::ItemRole)),         this,    SLOT(at_workbench_itemChanged(Qn::ItemRole)));
        disconnect(layout,      SIGNAL(itemAdded(QnWorkbenchItem *)),      this,    SLOT(at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *)));
        disconnect(layout,      SIGNAL(itemRemoved(QnWorkbenchItem *)),    this,    SLOT(at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem *)));
        disconnect(layout,      SIGNAL(zoomLinkAdded(QnWorkbenchItem*,QnWorkbenchItem*)),   this, SLOT(at_workbenchLayout_zoomLinkAdded(QnWorkbenchItem*,QnWorkbenchItem*)));
        disconnect(layout,      SIGNAL(zoomLinkRemoved(QnWorkbenchItem*,QnWorkbenchItem*)), this, SLOT(at_workbenchLayout_zoomLinkRemoved(QnWorkbenchItem*,QnWorkbenchItem*)));
        foreach (QnWorkbenchItem* item, layout->items()) {
             disconnect(item,   SIGNAL(dataChanged(int)),                  this,    SLOT(at_workbenchLayoutItem_dataChanged(int)));
        }

        sendMessage(QnVideoWallControlMessage(QnVideoWallControlMessage::ControlStopped));
        m_controlMode.active = active;
    }
}

void QnWorkbenchVideoWallHandler::setReviewMode(bool active) {
    if (m_reviewMode.active == active)
        return;

    QnWorkbenchLayout* layout = workbench()->currentLayout();
    if (active) {
        connect(layout,         SIGNAL(itemAdded(QnWorkbenchItem *)),      this,    SLOT(at_workbenchLayout_itemAdded_reviewMode(QnWorkbenchItem *)));
        connect(layout,         SIGNAL(itemRemoved(QnWorkbenchItem *)),    this,    SLOT(at_workbenchLayout_itemRemoved_reviewMode(QnWorkbenchItem *)));
        foreach (QnWorkbenchItem* item, layout->items()) {
             connect(item,      SIGNAL(geometryChanged()),                 this,    SLOT(at_workbenchLayoutItem_geometryChanged()));
        }

        m_reviewMode.active = active;
    } else {
        disconnect(layout,      SIGNAL(itemAdded(QnWorkbenchItem *)),      this,    SLOT(at_workbenchLayout_itemAdded_reviewMode(QnWorkbenchItem *)));
        disconnect(layout,      SIGNAL(itemRemoved(QnWorkbenchItem *)),    this,    SLOT(at_workbenchLayout_itemRemoved_reviewMode(QnWorkbenchItem *)));
        foreach (QnWorkbenchItem* item, layout->items()) {
             disconnect(item,      SIGNAL(geometryChanged()),              this,    SLOT(at_workbenchLayoutItem_geometryChanged()));
        }

        m_reviewMode.active = active;
    }
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

    connect(videoWall.data(), SIGNAL(itemChanged(const QnVideoWallResourcePtr&, const QnVideoWallItem&)), this, SLOT(at_videoWall_itemChanged(const QnVideoWallResourcePtr&, const QnVideoWallItem&)));
    connect(videoWall.data(), SIGNAL(itemRemoved(const QnVideoWallResourcePtr&, const QnVideoWallItem&)), this, SLOT(at_videoWall_itemRemoved(const QnVideoWallResourcePtr&, const QnVideoWallItem&)));

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
    connection()->saveAsync(videoWall);
}

void QnWorkbenchVideoWallHandler::at_attachToVideoWallAction_triggered() {
    if (!context()->user())
        return;

    QnVideoWallResourcePtr videoWall = menu()->currentParameters(sender()).resource().dynamicCast<QnVideoWallResource>();
    if(videoWall.isNull())
        return;    // TODO: #GDM VW implement videoWall selection / creation dialog

    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (snapshotManager()->isFile(layout))
        return; // TODO: #GDM VW action should not be available

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
    context()->resourcePool()->addResource(newLayout);

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

    if (snapshotManager()->isFile(layout))
        return; // TODO: #GDM VW action should not be available

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
    m_videoWallMode.instanceGuid = menu()->currentParameters(sender()).argument<QUuid>(Qn::VideoWallInstanceGuidRole);
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

        layout->setCellAspectRatio(1.0);
        layout->setCellSpacing(0.0, 0.0);
        layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission | Qn::WritePermission));
        layout->setData(Qn::VideoWallResourceRole, qVariantFromValue(videoWall));
        resourcePool()->addResource(layout);

        connect(videoWall.data(),   SIGNAL(itemAdded(const QnVideoWallResourcePtr &, const QnVideoWallItem &)),
                this,               SLOT(at_videoWall_itemAdded_inReviewMode(QnVideoWallResourcePtr,QnVideoWallItem)));
        connect(videoWall.data(),   SIGNAL(itemChanged(QnVideoWallResourcePtr,QnVideoWallItem)),
                this,               SLOT(at_videoWall_itemChanged_inReviewMode(QnVideoWallResourcePtr,QnVideoWallItem)));
        connect(videoWall.data(),   SIGNAL(itemRemoved(const QnVideoWallResourcePtr &, const QnVideoWallItem &)),
                this,               SLOT(at_videoWall_itemRemoved_inReviewMode(QnVideoWallResourcePtr,QnVideoWallItem)));

        QRect boundingRect;
        foreach(const QnVideoWallItem &source, videoWall->getItems())
            addScreenToReviewLayout(source, layout, boundingRect);

        menu()->trigger(Qn::OpenSingleLayoutAction, layout);
    }
}

void QnWorkbenchVideoWallHandler::at_saveVideoWallReviewAction_triggered() {
    QnWorkbenchLayout* layout = context()->workbench()->currentLayout();
    QnVideoWallResourcePtr videoWall = layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!videoWall)
        return;

    QSet<QUuid> movedPcs;

    foreach (QnWorkbenchItem* item, layout->items()) {
        QUuid itemUuid = item->data(Qn::VideoWallItemGuidRole).value<QUuid>();
        QUuid pcUuid = videoWall->getItem(itemUuid).pcUuid;
        if (movedPcs.contains(pcUuid))
            continue;
        movedPcs << pcUuid;

        QPoint screenPosition = item->data(Qn::ItemScreenPositionRole).value<QPoint>();
        QnVideoWallPcData pc = videoWall->getPc(pcUuid);
        pc.geometry.moveTopLeft(item->geometry().topLeft() - screenPosition);
        videoWall->updatePc(pcUuid, pc);
    }


    context()->snapshotManager()->setFlags(layout->resource(), context()->snapshotManager()->flags(layout->resource()) | Qn::ResourceIsBeingSaved);
    int handle = connection()->saveAsync(videoWall, this, SLOT(at_videoWall_saved(int,QnResourceList,int)));
    m_savingReviews[handle] = layout->resource();

}

void QnWorkbenchVideoWallHandler::at_videoWall_saved(int status, const QnResourceList &resources, int handle) {
    Q_UNUSED(resources)
    if (!m_savingReviews.contains(handle))
        return;

    QnLayoutResourcePtr layout = m_savingReviews.take(handle);
    context()->snapshotManager()->setFlags(layout, context()->snapshotManager()->flags(layout) & ~Qn::ResourceIsBeingSaved);

    if (status == 0)
        context()->snapshotManager()->setFlags(layout, context()->snapshotManager()->flags(layout) & ~Qn::ResourceIsChanged);
}

void QnWorkbenchVideoWallHandler::at_videoWall_layout_saved(int status, const QnResourceList &resources, int handle) {
    //TODO: #GDM VW remove handle from maps in case of unsuccessful requests
    if (status != 0)
        return;

    if (resources.size() == 0)
        return;

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
    if (resource->getGuid() != m_videoWallMode.guid.toString())
        return;

    if (m_videoWallMode.ready)
        return;
    m_videoWallMode.ready = true;
    submitDelayedItemOpen();
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

            if (QnVideoWallResourcePtr videoWall = layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>()) {
                disconnect(videoWall.data(),   SIGNAL(itemAdded(const QnVideoWallResourcePtr &, const QnVideoWallItem &)),
                            this,               SLOT(at_videoWall_itemAdded_inReviewMode(QnVideoWallResourcePtr,QnVideoWallItem)));
                disconnect(videoWall.data(),   SIGNAL(itemChanged(QnVideoWallResourcePtr,QnVideoWallItem)),
                            this,               SLOT(at_videoWall_itemChanged_inReviewMode(QnVideoWallResourcePtr,QnVideoWallItem)));
                disconnect(videoWall.data(),   SIGNAL(itemRemoved(const QnVideoWallResourcePtr &, const QnVideoWallItem &)),
                            this,               SLOT(at_videoWall_itemRemoved_inReviewMode(QnVideoWallResourcePtr,QnVideoWallItem)));
            }
        }

        if (QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>()) {
            int key = m_attaching.key(videoWall, -1);
            while (key > 0) {
                m_attaching.remove(key);
                key = m_attaching.key(videoWall, -1);
            }
        }

    }
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    if (videoWall->getGuid() != m_videoWallMode.guid.toString() || item.uuid != m_videoWallMode.instanceGuid)
        return;
    openVideoWallItem(videoWall);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    if (videoWall->getGuid() != m_videoWallMode.guid.toString() || item.uuid != m_videoWallMode.instanceGuid)
        return;
    closeInstance();
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemAdded_inReviewMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnWorkbenchLayout* layout = findReviewModeLayout(videoWall);
    if (!layout)
        return;

    QRect boundingRect = layout->boundingRect();
    addScreenToReviewLayout(item, layout->resource(), boundingRect);
}

void QnWorkbenchVideoWallHandler::at_videoWall_itemChanged_inReviewMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnWorkbenchLayout* layout = findReviewModeLayout(videoWall);
    if (!layout)
        return;

    foreach (QnWorkbenchItem* workbenchItem, layout->items()) {
        if (workbenchItem->data(Qn::VideoWallItemGuidRole).toUuid() != item.uuid)
            continue;
        QRect oldGeometry = workbenchItem->geometry();
        layout->removeItem(workbenchItem);
        QRect boundingRect = layout->boundingRect();
        addScreenToReviewLayout(item, layout->resource(), boundingRect, oldGeometry);
        break;
    }


}

void QnWorkbenchVideoWallHandler::at_videoWall_itemRemoved_inReviewMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    QnWorkbenchLayout* layout = findReviewModeLayout(videoWall);
    if (!layout)
        return;

    foreach (QnWorkbenchItem* workbenchItem, layout->items()) {
        if (workbenchItem->data(Qn::VideoWallItemGuidRole).toUuid() != item.uuid)
            continue;
        layout->removeItem(workbenchItem);
        break;
    }

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
            connect(mediaWidget, SIGNAL(motionSelectionChanged()), this, SLOT(at_widget_motionSelectionChanged()));
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
    disconnect(workbench()->currentLayout(), 0, this, 0);
    setControlMode(false);
}

void QnWorkbenchVideoWallHandler::at_workbench_currentLayoutChanged() {
    connect(workbench()->currentLayout(), SIGNAL(dataChanged(int)), this, SLOT(at_workbenchLayout_dataChanged(int)));
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
    connect(item, SIGNAL(dataChanged(int)),     this,   SLOT(at_workbenchLayoutItem_dataChanged(int)));

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemAdded);
    message[uuidKey] = item->uuid().toString();
    message[resourceKey] = item->resourceUid();
    message[geometryKey] = QString::fromUtf8(QJson::serialized(item->geometry()));
    message[zoomRectKey] = QString::fromUtf8(QJson::serialized(item->zoomRect()));
    message[rotationKey] = QString::fromUtf8(QJson::serialized(item->rotation()));
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem *item) {
    disconnect(item, SIGNAL(dataChanged(int)),     this,   SLOT(at_workbenchLayoutItem_dataChanged(int)));

    if (!m_controlMode.active)
        return;

    QnVideoWallControlMessage message(QnVideoWallControlMessage::LayoutItemRemoved);
    message[uuidKey] = item->uuid().toString();
    sendMessage(message);
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemAdded_reviewMode(QnWorkbenchItem *item) {
    connect(item, SIGNAL(geometryChanged()),    this,   SLOT(at_workbenchLayoutItem_geometryChanged()));
}

void QnWorkbenchVideoWallHandler::at_workbenchLayout_itemRemoved_reviewMode(QnWorkbenchItem *item) {
    disconnect(item, SIGNAL(geometryChanged()),    this,   SLOT(at_workbenchLayoutItem_geometryChanged()));
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
    case Qn::ItemFrameColorRole:
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

void QnWorkbenchVideoWallHandler::at_workbenchLayoutItem_geometryChanged() {
    if (!m_reviewMode.active)
        return;
    if (m_reviewMode.inGeometryChange)
        return;

    QnWorkbenchLayout* layout = context()->workbench()->currentLayout();
    QnVideoWallResourcePtr videoWall = layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    if (!videoWall)
        return;

    m_reviewMode.inGeometryChange = true;

    QnWorkbenchItem* item = static_cast<QnWorkbenchItem *>(sender());
    QUuid itemUuid = item->data(Qn::VideoWallItemGuidRole).value<QUuid>();
    QUuid pcUuid = videoWall->getItem(itemUuid).pcUuid;
    QPoint screenPosition = item->data(Qn::ItemScreenPositionRole).value<QPoint>();

    foreach (QnWorkbenchItem* otherItem, layout->items()) {
        if (otherItem == item)
            continue;

        QUuid otherItemUuid = otherItem->data(Qn::VideoWallItemGuidRole).value<QUuid>();
        QUuid otherPcUuid = videoWall->getItem(otherItemUuid).pcUuid;
        if (otherPcUuid != pcUuid)
            continue;

        QPoint otherScreenPosition = otherItem->data(Qn::ItemScreenPositionRole).value<QPoint>();
        QRect otherGeometry = item->geometry();
        otherGeometry.moveTopLeft(otherGeometry.topLeft() + otherScreenPosition - screenPosition);
        otherItem->setGeometry(otherGeometry);
    }

    m_reviewMode.inGeometryChange = false;

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

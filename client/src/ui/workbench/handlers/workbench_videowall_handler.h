#ifndef WORKBENCH_VIDEOWALL_HANDLER_H
#define WORKBENCH_VIDEOWALL_HANDLER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QUuid>

#include <api/app_server_connection.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchItem;
class QnResourceWidget;

class QnWorkbenchVideoWallHandler : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchVideoWallHandler(QObject *parent = 0);
    virtual ~QnWorkbenchVideoWallHandler();

private:
    QnAppServerConnectionPtr connection() const;

    QRect calculateSnapGeometry();

    void attachLayout(const QnVideoWallResourcePtr &videoWall, const QnId &layoutId);
    void resetLayout(const QnVideoWallItemIndexList &items, const QnId &layoutId);

    void openNewWindow(const QStringList &args);
    void openVideoWallItem(const QnVideoWallResourcePtr &videoWall);
    void closeInstance();
    void sendInstanceGuid();

    void setControlMode(bool active);
    void setReviewMode(bool active);
    void updateMode();

    void sendMessage(QnVideoWallControlMessage message);
    void handleMessage(const QnVideoWallControlMessage &message, const QUuid &controllerUuid = QUuid(), qint64 sequence = -1 );
    void storeMessage(const QnVideoWallControlMessage &message, const QUuid &controllerUuid, qint64 sequence);
    void restoreMessages(const QUuid &controllerUuid, qint64 sequence);

    void addScreenToReviewLayout(const QnVideoWallItem &source, const QnLayoutResourcePtr &layout, QRect &boundingRect, QRect geometry = QRect());

    /** Returns list of target videowall items for current layout. */
    QnVideoWallItemIndexList targetList() const;

    QnWorkbenchLayout* findReviewModeLayout(const QnVideoWallResourcePtr &videoWall) const;

private slots:
    void at_connection_opened();
    void at_context_userChanged();

    void at_newVideoWallAction_triggered();
    void at_attachToVideoWallAction_triggered();
    void at_detachFromVideoWallAction_triggered();
    void at_resetVideoWallLayoutAction_triggered();
    void at_deleteVideoWallItemAction_triggered();
    void at_startVideoWallAction_triggered();
    void at_stopVideoWallAction_triggered();
    void at_delayedOpenVideoWallItemAction_triggered();
    void at_renameAction_triggered();
    void at_identifyVideoWallAction_triggered();
    void at_addVideoWallItemsToUserAction_triggered();
    void at_startVideoWallControlAction_triggered();
    void at_openVideoWallsReviewAction_triggered();
    void at_saveVideoWallReviewAction_triggered();

    void at_videoWall_saved(int status, const QnResourceList &resources, int handle);
    void at_videoWall_layout_saved(int status, const QnResourceList &resources, int handle);

    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

    void at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

    void at_videoWall_itemAdded_inReviewMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemChanged_inReviewMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemRemoved_inReviewMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

    void at_eventManager_controlMessageReceived(const QnVideoWallControlMessage &message);

    void at_display_widgetAdded(QnResourceWidget* widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget* widget);
    void at_widget_motionSelectionChanged();

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *item);
    void at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem *item);
    void at_workbenchLayout_itemAdded_reviewMode(QnWorkbenchItem *item);
    void at_workbenchLayout_itemRemoved_reviewMode(QnWorkbenchItem *item);
    void at_workbenchLayout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_workbenchLayout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_workbenchLayout_dataChanged(int role);

    void at_workbenchLayoutItem_dataChanged(int role);
    void at_workbenchLayoutItem_geometryChanged();

    void at_navigator_positionChanged();
    void at_navigator_speedChanged();

    void at_workbenchStreamSynchronizer_runningChanged();

    void submitDelayedItemOpen();

private:
    typedef QHash<qint64, QnVideoWallControlMessage> StoredMessagesHash;

    struct {
        bool active;        /**< Client is run in videowall mode. */
        bool ready;         /**< Resources are loaded. */
        bool opening;       /**< We are currently opening videowall. */
        QUuid guid;
        QUuid instanceGuid;
        QHash<QUuid, qint64> sequenceByPcUuid;
        QHash<QUuid, StoredMessagesHash> storedMessages;
    }  m_videoWallMode;

    struct {
        QList<int> left;
        QList<int> right;
        QList<int> top;
        QList<int> bottom;
        bool calculated;
    } m_screenSnaps;

    struct {
        bool active;
        QString pcUuid;
        qint64 sequence;
    } m_controlMode;

    struct {
        bool active;
        bool inGeometryChange;
    } m_reviewMode;


    QHash<int, QnVideoWallResourcePtr> m_attaching;
    QHash<int, QnVideoWallItemIndexList> m_resetting;
    QHash<int, QnLayoutResourcePtr> m_savingReviews;
};

#endif // WORKBENCH_VIDEOWALL_HANDLER_H

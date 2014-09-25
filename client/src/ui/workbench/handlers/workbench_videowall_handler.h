#ifndef WORKBENCH_VIDEOWALL_HANDLER_H
#define WORKBENCH_VIDEOWALL_HANDLER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <utils/common/uuid.h>

#include <api/app_server_connection.h>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_pc_data.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchItem;
class QnResourceWidget;
class QTimer;
class QnVideoWallLicenseUsageHelper;

class QnWorkbenchVideoWallHandler : public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    explicit QnWorkbenchVideoWallHandler(QObject *parent = 0);
    virtual ~QnWorkbenchVideoWallHandler();

    bool saveReviewLayout(const QnLayoutResourcePtr &layout, std::function<void(int, ec2::ErrorCode)> callback);
    bool saveReviewLayout(QnWorkbenchLayout *layout, std::function<void(int, ec2::ErrorCode)> callback);

private:
    enum class ItemAction {
        Added,
        Changed,
        Removed
    };

    ec2::AbstractECConnectionPtr connection2() const;

    void resetLayout(const QnVideoWallItemIndexList &items, const QnLayoutResourcePtr &layout);
    void swapLayouts(const QnVideoWallItemIndex firstIndex, const QnLayoutResourcePtr &firstLayout, const QnVideoWallItemIndex &secondIndex, const QnLayoutResourcePtr &secondLayout);

    /** Updates item's layout with provided value. Provided layout should be saved. */
    void updateItemsLayout(const QnVideoWallItemIndexList &items, const QnUuid &layoutId);

    bool canStartVideowall(const QnVideoWallResourcePtr &videowall);

    void startVideowallAndExit(const QnVideoWallResourcePtr &videoWall);

    void openNewWindow(const QStringList &args);
    void openVideoWallItem(const QnVideoWallResourcePtr &videoWall);
    void closeInstanceDelayed();

    bool canStartControlMode() const;
    void setControlMode(bool active);
    void updateMode();

    void sendMessage(QnVideoWallControlMessage message, bool cached = false);
    void handleMessage(const QnVideoWallControlMessage &message, const QnUuid &controllerUuid = QnUuid(), qint64 sequence = -1 );
    void storeMessage(const QnVideoWallControlMessage &message, const QnUuid &controllerUuid, qint64 sequence);
    void restoreMessages(const QnUuid &controllerUuid, qint64 sequence);

    /** Returns list of target videowall items for current layout. */
    QnVideoWallItemIndexList targetList() const;

    QnLayoutResourcePtr findExistingResourceLayout(const QnResourcePtr &resource) const;
    QnLayoutResourcePtr constructLayout(const QnResourceList &resources) const;

    static QString shortcutPath();
    bool shortcutExists(const QnVideoWallResourcePtr &videowall) const;
    bool createShortcut(const QnVideoWallResourcePtr &videowall);
    bool deleteShortcut(const QnVideoWallResourcePtr &videowall);

    void setItemOnline(const QnUuid &instanceGuid, bool online);
    void updateMainWindowGeometry(const QnScreenSnaps &screenSnaps);

    void updateControlLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action);
    void updateReviewLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action);

    bool validateLicenses(const QString &detail) const;
private slots:

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
    void at_startVideoWallControlAction_triggered();
    void at_openVideoWallsReviewAction_triggered();
    void at_saveCurrentVideoWallReviewAction_triggered();
    void at_saveVideoWallReviewAction_triggered();
    void at_dropOnVideoWallItemAction_triggered();
    void at_pushMyScreenToVideowallAction_triggered();
    void at_videowallSettingsAction_triggered();
    void at_saveVideowallMatrixAction_triggered();
    void at_loadVideowallMatrixAction_triggered();
    void at_deleteVideowallMatrixAction_triggered();

    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

    void at_videoWall_pcAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc);
    void at_videoWall_pcChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc);
    void at_videoWall_pcRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc);

    void at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

    void at_videoWall_itemChanged_activeMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemRemoved_activeMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

    void at_eventManager_controlMessageReceived(const QnVideoWallControlMessage &message);

    void at_display_widgetAdded(QnResourceWidget* widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget* widget);

    void at_widget_motionSelectionChanged();
    void at_widget_dewarpingParamsChanged();

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem *item);
    void at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem *item);
    void at_workbenchLayout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_workbenchLayout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_workbenchLayout_dataChanged(int role);

    void at_workbenchLayoutItem_dataChanged(int role);

    void at_navigator_positionChanged();
    void at_navigator_speedChanged();

    void at_workbenchStreamSynchronizer_runningChanged();

    void at_controlModeCacheTimer_timeout();

    void submitDelayedItemOpen();

    void saveVideowall(const QnVideoWallResourcePtr& videowall, bool saveLayout = false);
    void saveVideowalls(const QSet<QnVideoWallResourcePtr> &videowalls, bool saveLayout = false);
    void saveVideowallAndReviewLayout(const QnVideoWallResourcePtr& videowall, const QnLayoutResourcePtr &layout = QnLayoutResourcePtr());
private:
    typedef QHash<qint64, QnVideoWallControlMessage> StoredMessagesHash;

    struct {
        bool active;        /**< Client is run in videowall mode. */
        bool ready;         /**< Resources are loaded. */
        bool opening;       /**< We are currently opening videowall. */
        QnUuid guid;
        QnUuid instanceGuid; /**< Guid of the videowall item we are currently displaying. */
        QHash<QnUuid, qint64> sequenceByPcUuid;
        QHash<QnUuid, StoredMessagesHash> storedMessages;
    }  m_videoWallMode;


    struct {
        bool active;
        QString pcUuid;
        qint64 sequence;
        QList<QnVideoWallControlMessage> cachedMessages;
        QTimer* cacheTimer;
    } m_controlMode;

    QScopedPointer<QnVideoWallLicenseUsageHelper> m_licensesHelper;
};

#endif // WORKBENCH_VIDEOWALL_HANDLER_H

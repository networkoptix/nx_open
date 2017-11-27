#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_pc_data.h>

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnResourceWidget;
class QTimer;
class QnVideoWallLicenseUsageHelper;
class QnUuidPool;

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

    /** Place first layout to the first item, second layout to the second item. Save layouts if needed. */
    void swapLayouts(const QnVideoWallItemIndex firstIndex, const QnLayoutResourcePtr &firstLayout, const QnVideoWallItemIndex &secondIndex, const QnLayoutResourcePtr &secondLayout);

    /** Updates item's layout with provided value. Provided layout should be saved. */
    void updateItemsLayout(const QnVideoWallItemIndexList &items, const QnUuid &layoutId);

    bool canStartVideowall(const QnVideoWallResourcePtr &videowall) const;

    void switchToVideoWallMode(const QnVideoWallResourcePtr &videoWall);

    void openNewWindow(const QStringList &args);
    void openVideoWallItem(const QnVideoWallResourcePtr &videoWall);
    void closeInstanceDelayed();

    bool canStartControlMode() const;
    void setControlMode(bool active);
    void updateMode();

    void sendMessage(const QnVideoWallControlMessage& message, bool cached = false);
    void handleMessage(const QnVideoWallControlMessage &message, const QnUuid &controllerUuid = QnUuid(), qint64 sequence = -1 );
    void storeMessage(const QnVideoWallControlMessage &message, const QnUuid &controllerUuid, qint64 sequence);
    void restoreMessages(const QnUuid &controllerUuid, qint64 sequence);

    /** Returns list of target videowall items for current layout. */
    QnVideoWallItemIndexList targetList() const;

    QnLayoutResourcePtr constructLayout(const QnResourceList &resources) const;
    void cleanupUnusedLayouts();

    void setItemOnline(const QnUuid &instanceGuid, bool online);
    void setItemControlledBy(const QnUuid &layoutId, const QnUuid &controllerId, bool on);
    void updateMainWindowGeometry(const QnScreenSnaps &screenSnaps);

    void updateControlLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action);
    void updateReviewLayout(const QnVideoWallResourcePtr &videowall, const QnVideoWallItem &item, ItemAction action);

    /**
     * Check if layout contains local files, which cannot be placed on the given screen.
     * Displays an error message if there are local files, and screen belongs to other pc.
     */
    bool checkLocalFiles(const QnVideoWallItemIndex& index, const QnLayoutResourcePtr& layout);

    bool validateLicenses(const QString &detail) const;

    /** Returns id of the running client that is currently controlling provided layout. */
    QnUuid getLayoutController(const QnUuid &layoutId);

    /** Check if user will not lose access to resources if remove them from layout. */
    bool confirmRemoveResourcesFromLayout(const QnLayoutResourcePtr& layout,
        const QnResourceList& resources) const;

private slots:
    void at_newVideoWallAction_triggered();
    void at_attachToVideoWallAction_triggered();
    void at_detachFromVideoWallAction_triggered();
    void at_deleteVideoWallItemAction_triggered();
    void at_startVideoWallAction_triggered();
    void at_stopVideoWallAction_triggered();
    void at_delayedOpenVideoWallItemAction_triggered();
    void at_renameAction_triggered();
    void at_identifyVideoWallAction_triggered();
    void at_startVideoWallControlAction_triggered();
    void at_openVideoWallReviewAction_triggered();
    void at_saveCurrentVideoWallReviewAction_triggered();
    void at_saveVideoWallReviewAction_triggered();
    void at_dropOnVideoWallItemAction_triggered();
    void at_pushMyScreenToVideowallAction_triggered();
    void at_videowallSettingsAction_triggered();
    void at_saveVideowallMatrixAction_triggered();
    void at_loadVideowallMatrixAction_triggered();
    void at_deleteVideowallMatrixAction_triggered();
    void at_radassAction_triggered();

    void at_resPool_resourceAdded(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

    void at_videoWall_pcAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc);
    void at_videoWall_pcChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc);
    void at_videoWall_pcRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallPcData &pc);

    void at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);
    void at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

    void at_videoWall_itemChanged_activeMode(const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);
    void at_videoWall_itemRemoved_activeMode(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);

    void at_eventManager_controlMessageReceived(const ec2::ApiVideowallControlMessageData& message);

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

    void at_workbenchLayoutItem_dataChanged(Qn::ItemDataRole role);

    void at_navigator_positionChanged();
    void at_navigator_playingChanged();
    void at_navigator_speedChanged();

    void at_workbenchStreamSynchronizer_runningChanged();

    void at_controlModeCacheTimer_timeout();

    void submitDelayedItemOpen();

    void saveVideowall(const QnVideoWallResourcePtr& videowall, bool saveLayout = false);
    void saveVideowalls(const QSet<QnVideoWallResourcePtr> &videowalls, bool saveLayout = false);
    void saveVideowallAndReviewLayout(const QnVideoWallResourcePtr& videowall, const QnLayoutResourcePtr &layout = QnLayoutResourcePtr());

private:
    void showControlledByAnotherUserMessage() const;

    void showFailedToApplyChanges() const;

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

    QnVideoWallLicenseUsageHelper* m_licensesHelper;
    QScopedPointer<QnUuidPool> m_uuidPool;
};

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_pc_data.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/utils/multiscreen_widget_geometry_setter.h>
#include <nx/vms/license/license_usage_fwd.h>
#include <nx_ec/ec_api_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnResourceWidget;
class QTimer;
class UuidPool;

namespace nx::vms::api { struct VideowallControlMessageData; }

class QnWorkbenchVideoWallHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QObject;
    using VideoWallCallbackFunction =
        nx::vms::client::desktop::ResourcesChangesManager::VideoWallCallbackFunction;

public:
    explicit QnWorkbenchVideoWallHandler(QObject* parent = 0);
    virtual ~QnWorkbenchVideoWallHandler();

    void filterAllowedMediaResources(QnResourceList& resources) const;
    void filterAllowedLayoutItems(const nx::vms::client::core::LayoutResourcePtr& layout) const;

private:
    enum class ItemAction
    {
        Added,
        Changed,
        Removed
    };

    void resetLayout(const QnVideoWallItemIndexList& items,
        const nx::vms::client::core::LayoutResourcePtr& layout);

    /**
     * Place first layout to the first item, second layout to the second item. Save layouts if
     * needed.
     */
    void swapLayouts(
        const QnVideoWallItemIndex firstIndex,
        const nx::vms::client::core::LayoutResourcePtr& firstLayout,
        const QnVideoWallItemIndex& secondIndex,
        const nx::vms::client::core::LayoutResourcePtr& secondLayout);

    /** Updates item's layout with provided value. Provided layout should be saved. */
    void updateItemsLayout(const QnVideoWallItemIndexList& items, const nx::Uuid& layoutId);

    bool canStartVideowall(const QnVideoWallResourcePtr& videowall) const;

    void switchToVideoWallMode(const QnVideoWallResourcePtr& videoWall);

    void launchItems(const QnVideoWallResourcePtr& videoWall);

    void openNewWindow(
        nx::Uuid videoWallId,
        const QnVideoWallItem& item,
        const nx::utils::SoftwareVersion& version = {});

    void openVideoWallItem(const QnVideoWallResourcePtr& videoWall);
    void closeInstanceDelayed();

    /** Check if current client instance can start controlling the given videowall layout. */
    bool canStartControlMode(const nx::Uuid& layoutId) const;

    /** Enable/disable videowall direct control mode on the currently opened layout. */
    void setControlMode(bool active);

    void updateMode();

    void sendMessage(const QnVideoWallControlMessage& message, bool cached = false);

    void handleMessage(
        const QnVideoWallControlMessage& message,
        const nx::Uuid& controllerUuid = nx::Uuid(),
        qint64 sequence = -1);

    void storeMessage(
        const QnVideoWallControlMessage& message,
        const nx::Uuid& controllerUuid,
        qint64 sequence);

    void restoreMessages(const nx::Uuid& controllerUuid, qint64 sequence);

    /** Returns list of target videowall items for current layout. */
    QnVideoWallItemIndexList targetList() const;

    nx::vms::client::core::LayoutResourcePtr constructLayout(
        const QnResourceList& resources) const;
    void cleanupUnusedLayouts(const QnVideoWallResourceList& videowalls = {});

    void setItemOnline(const nx::Uuid& instanceGuid, bool online);
    void setItemControlledBy(const nx::Uuid& layoutId, const nx::Uuid& controllerId, bool on);
    void updateMainWindowGeometry(const QnScreenSnaps& screenSnaps);

    void updateControlLayout(const QnVideoWallItem& item, ItemAction action);
    void updateReviewLayout(
        const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& item,
        ItemAction action);

    /**
     * Check if layout contains local files, which cannot be placed on the given screen.
     * Displays an error message if there are local files, and screen belongs to other pc.
     */
    bool checkLocalFiles(const QnVideoWallItemIndex& index, const QnLayoutResourcePtr& layout);

    /**
     * Show licenses error dialog with a corresponding text.
     */
    void showLicensesErrorDialog(const QString& detail) const;

    /** Returns id of the running client that is currently controlling provided layout. */
    nx::Uuid getLayoutController(const nx::Uuid& layoutId);

    /** Check if user will not lose access to resources if remove them from layout. */
    bool confirmRemoveResourcesFromLayout(
        const QnLayoutResourcePtr& layout,
        const QnResourceList& resources) const;

    void at_newVideoWallAction_triggered();
    void at_deleteVideoWallAction_triggered();
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
    void at_videoWallScreenSettingsAction_triggered();
    void at_radassAction_triggered();

    void at_resPool_resourceAdded(const QnResourcePtr& resource);
    void at_resPool_resourceRemoved(const QnResourcePtr& resource);

    void at_videoWall_pcAdded(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallPcData& pc);
    void at_videoWall_pcChanged(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallPcData& pc);
    void at_videoWall_pcRemoved(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallPcData& pc);

    void at_videoWall_itemAdded(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);
    void at_videoWall_itemChanged(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);
    void at_videoWall_itemRemoved(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);

    void at_videoWall_itemChanged_activeMode(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);
    void at_videoWall_itemRemoved_activeMode(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);

    void at_eventManager_controlMessageReceived(
        const nx::vms::api::VideowallControlMessageData& message);

    void at_display_widgetAdded(QnResourceWidget* widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget* widget);

    void at_widget_motionSelectionChanged();
    void at_widget_dewarpingParamsChanged();

    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();
    void at_workbench_itemChanged(Qn::ItemRole role);

    void at_workbenchLayout_itemAdded_controlMode(QnWorkbenchItem* item);
    void at_workbenchLayout_itemRemoved_controlMode(QnWorkbenchItem* item);
    void at_workbenchLayout_zoomLinkAdded(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem);
    void at_workbenchLayout_zoomLinkRemoved(
        QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem);
    void at_workbenchLayout_dataChanged(int role);

    void at_workbenchLayoutItem_dataChanged(int role);

    void at_navigator_playingChanged();
    void at_navigator_speedChanged();

    void at_workbenchStreamSynchronizer_runningChanged();

    void at_controlModeCacheTimer_timeout();

    void submitDelayedItemOpen();

    /**
     * Send the message to sync timeline position. Target client will display timeline on each
     * position change.
     * @param silent Set to true when position is synced automatically, so target client can
     *     skip timeline displaying.
     */
    void syncTimelinePosition(bool silent);

    void saveVideowall(const QnVideoWallResourcePtr& videowall, bool saveLayout = false,
        VideoWallCallbackFunction callback = {});
    void saveVideowalls(const QSet<QnVideoWallResourcePtr>& videowalls, bool saveLayout = false,
        VideoWallCallbackFunction callback = {});
    void saveVideowallAndReviewLayout(
        const QnVideoWallResourcePtr& videowall,
        nx::vms::client::core::LayoutResourcePtr reviewLayout = {},
        VideoWallCallbackFunction callback = {});

private:
    void showControlledByAnotherUserMessage() const;
    void showFailedToApplyChanges() const;
    QString desktopCameraPhysicalId() const;
    nx::Uuid desktopCameraLayoutId() const;

private:
    typedef QHash<qint64, QnVideoWallControlMessage> StoredMessagesHash;

    struct
    {
        bool active; /**< Client is run in videowall mode. */
        bool ready; /**< Resources are loaded. */
        bool opening; /**< We are currently opening videowall. */
        nx::Uuid guid;
        nx::Uuid instanceGuid; /**< Guid of the videowall item we are currently displaying. */
        QHash<nx::Uuid, qint64> sequenceByPcUuid;
        QHash<nx::Uuid, StoredMessagesHash> storedMessages;
    } m_videoWallMode;

    struct
    {
        bool active;
        QString pcUuid;
        qint64 sequence;
        QList<QnVideoWallControlMessage> cachedMessages;
        QTimer* cacheTimer;
        QTimer* syncPlayTimer; //< Timer for periodical play position sync.
    } m_controlMode;

    nx::vms::license::VideoWallLicenseUsageHelper* m_licensesHelper;
    QScopedPointer<UuidPool> m_uuidPool;
    QScopedPointer<MultiscreenWidgetGeometrySetter> m_geometrySetter;

    QnVirtualCameraResourcePtr m_localDesktopCamera;
    QSet<nx::Uuid> m_videoWallsWithLocalDesktopCamera;
    QSet<nx::Uuid> m_localDesktopCameraLayoutIds;
};

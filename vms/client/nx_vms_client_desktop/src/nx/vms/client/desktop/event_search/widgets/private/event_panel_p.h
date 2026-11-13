// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>

#include "../event_panel.h"

class QTabWidget;
class AnimationTimer;
class VariantAnimator;
class HoverFocusProcessor;

namespace nx::vms::client::desktop {

class ThumbnailTooltip;
class NotificationListWidget;
class NotificationCounterLabel;
class AbstractSearchWidget;
class SimpleMotionSearchWidget;
class BookmarkSearchWidget;
class EventSearchWidget;
class AnalyticsSearchWidget;
class MultiImageProvider;
class AbstractSearchSynchronizer;
class OverlappableSearchWidget;
class AnimatedCompactTabWidget;
class NotificationBellWidget;
class CallAlarmManager;

class EventPanel::Private:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

    Tab currentTab() const;
    bool setCurrentTab(Tab tab, bool updateRequestedTab = true);

    AbstractSearchWidget* currentSearchWidget() const;

signals:
    void tooltipClicked();

private:
    void rebuildTabs();
    void updateUnreadCounter(int count, nx::vms::event::Level importance);
    void showContextMenu(const QPoint& pos);
    void setTabCurrent(QWidget* tab, bool current);
    void at_eventTileHovered(const QModelIndex& index, EventTile* tile);

    struct TooltipPreviews
    {
        /** Cameras whose previews the tooltip shows. */
        QnVirtualCameraResourceList cameras;

        /** Number of the event previews the user can see, including the tile one. */
        int previewCount = 0;
    };

    std::unique_ptr<MultiImageProvider> multiImageProvider(
        const QModelIndex& index, const QnVirtualCameraResourceList& cameras) const;
    TooltipPreviews tooltipPreviews(const QModelIndex& index, const EventTile* tile) const;
    void setupTooltipCounters(MultiImageProvider* provider, const TooltipPreviews& previews);

private:
    class StateDelegate;

    struct SynchronizerData
    {
        QWidget* tab;
        const AbstractSearchWidget* searchWidget;
        AbstractSearchSynchronizer* synchronizer;
    };

    EventPanel* const q;
    AnimatedCompactTabWidget* m_tabs = nullptr;
    Tab m_requestedTab = Tab::notifications; //< Used to restore session state.
    bool m_suppressRequestedTabUpdate = false;

    NotificationListWidget* m_notificationsTab = nullptr;
    NotificationCounterLabel* m_counterLabel = nullptr;

    SimpleMotionSearchWidget* m_motionTab = nullptr;
    OverlappableSearchWidget* m_bookmarksTab = nullptr;
    OverlappableSearchWidget* m_vmsEventsTab = nullptr;
    OverlappableSearchWidget* m_analyticsTab = nullptr;

    QList<SynchronizerData> m_synchronizers;
    QHash<QWidget*, Tab> m_tabIds;

    std::unique_ptr<CallAlarmManager> m_callAlarmManager;
    QPointer<NotificationBellWidget> m_notificationBellWidget;

    std::unique_ptr<ThumbnailTooltip> m_tooltip;

    // Snapshots of the current tooltip. Declared after the tooltip, so it is destroyed before.
    std::unique_ptr<MultiImageProvider> m_tooltipImageProvider;

    // Required to correctly display tooltip.
    EventTile* m_lastHoveredTile = nullptr;
    std::unique_ptr<QObject> hoveredTilePositionWatcher;
    nx::utils::ScopedConnections m_tooltipCounterConnections;

    nx::utils::ScopedConnections m_connections; //< Connections that should be guarded.
};

} // namespace nx::vms::client::desktop

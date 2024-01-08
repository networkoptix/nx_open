// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <nx/utils/scoped_connections.h>

#include "../event_panel.h"

class QTabWidget;
class AnimationTimer;
class VariantAnimator;
class HoverFocusProcessor;

namespace QnNotificationLevel { enum class Value; }

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
class NotificationBellManager;

class EventPanel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

    Tab currentTab() const;
    bool setCurrentTab(Tab tab);

signals:
    void tooltipClicked();

private:
    void rebuildTabs();
    void updateUnreadCounter(int count, QnNotificationLevel::Value importance);
    void showContextMenu(const QPoint& pos);
    void setTabCurrent(QWidget* tab, bool current);
    void at_eventTileHovered(const QModelIndex& index, EventTile* tile);
    std::unique_ptr<MultiImageProvider> multiImageProvider(
            const QModelIndex& index) const;

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

    NotificationListWidget* m_notificationsTab = nullptr;
    NotificationCounterLabel* m_counterLabel = nullptr;

    SimpleMotionSearchWidget* m_motionTab = nullptr;
    OverlappableSearchWidget* m_bookmarksTab = nullptr;
    OverlappableSearchWidget* m_eventsTab = nullptr;
    OverlappableSearchWidget* m_vmsEventsTab = nullptr;
    OverlappableSearchWidget* m_analyticsTab = nullptr;

    QList<SynchronizerData> m_synchronizers;
    QHash<QWidget*, Tab> m_tabIds;

    QWidget* m_previousTab = nullptr;
    QWidget* m_lastTab = nullptr;

    NotificationBellWidget* m_notificationBellWidget = nullptr;
    NotificationBellManager* m_notificationBellManager = nullptr;

    std::unique_ptr<ThumbnailTooltip> m_tooltip;

    // Required to correctly display tooltip.
    EventTile* m_lastHoveredTile = nullptr;
    std::unique_ptr<QObject> hoveredTilePositionWatcher;

    nx::utils::ScopedConnections m_connections; //< Connections that should be guarded.

    Tab m_requestedTab = Tab::notifications; //< Used to restore session state.
};

} // namespace nx::vms::client::desktop

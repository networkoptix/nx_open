# Right Panel architecture {#right_panel}

[EventPanel]: @ref nx::vms::client::desktop::EventPanel
[EventRibbon]: @ref nx::vms::client::desktop::EventRibbon
[EventTile]: @ref nx::vms::client::desktop::EventTile
[SystemHealthListModel]: @ref nx::vms::client::desktop::SystemHealthListModel
[ProgressListModel]: @ref nx::vms::client::desktop::ProgressListModel
[NotificationListModel]: @ref nx::vms::client::desktop::NotificationListModel
[ConcatenationListModel]: @ref nx::vms::client::desktop::ConcatenationListModel
[SimpleMotionSearchListModel]: @ref nx::vms::client::desktop::SimpleMotionSearchListModel
[BookmarkSearchListModel]: @ref nx::vms::client::desktop::BookmarkSearchListModel
[EventSearchListModel]: @ref nx::vms::client::desktop::EventSearchListModel
[AnalyticsSearchListModel]: @ref nx::vms::client::desktop::AnalyticsSearchListModel
[NotificationListWidget]: @ref nx::vms::client::desktop::NotificationListWidget
[SimpleMotionSearchWidget]: @ref nx::vms::client::desktop::SimpleMotionSearchWidget
[BookmarkSearchWidget]: @ref nx::vms::client::desktop::BookmarkSearchWidget
[EventSearchWidget]: @ref nx::vms::client::desktop::EventSearchWidget
[AnalyticsSearchWidget]: @ref nx::vms::client::desktop::AnalyticsSearchWidget
[MotionSearchSynchronizer]: @ref nx::vms::client::desktop::MotionSearchSynchronizer
[BookmarkSearchSynchronizer]: @ref nx::vms::client::desktop::BookmarkSearchSynchronizer
[AnalyticsSearchSynchronizer]: @ref nx::vms::client::desktop::AnalyticsSearchSynchronizer

Right Panel is a pane at the right side of viewport that displays realtime notifications and 
system events of different kind, as well as provides means to lookup past events in the archive. 
Archive lookup is implemented as fetch-on-demand, loading data in batches when more data 
is requested.

The panel consists of several tabs, each of them displaying vertically scrollable ribbon of tiles.
The panel is represented by [EventPanel] class, the ribbon by [EventRibbon] class, and a tile
by [EventTile] class.

Tabs synchronize their state with **Workbench** state (timeline mode, current camera display mode
etc.)

@subpage right_panel_models

@subpage right_panel_widgets

@subpage right_panel_synchronizers

## Notifications tab
Represented by [NotificationListWidget]. Displays system health informers, progress informers and
system notifications. It uses a concatenation of [SystemHealthListModel], [ProgressListModel] and
[NotificationListModel]. Concatenation is done via utility [ConcatenationListModel].

## Motion tab
Represented by [SimpleMotionSearchWidget]. Complements motion search mode and displays tiles 
corresponding to motion chunks. Uses [SimpleMotionSearchListModel]. Is synchronized with
**Workbench** state by [MotionSearchSynchronizer].

## Bookmarks tab
Represented by [BookmarkSearchWidget]. Displays bookmarks. Uses [BookmarkSearchListModel].
Is synchronized with **Workbench** state by [BookmarkSearchSynchronizer].

## Events tab
Represented by [EventSearchWidget]. Displays events recorded to system event log. Uses 
[EventSearchListModel]. Does not require a **Workbench** state synchronizer.

## Detected objects tab
Represented by [AnalyticsSearchWidget]. Displays object detection events recorded to the
archive as well as those events happening in realtime. Uses [AnalyticsSearchListModel].
Is synchronized with **Workbench** state by [AnalyticsSearchSynchronizer].

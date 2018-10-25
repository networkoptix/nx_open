# Right Panel architecture {#right_panel}

[EventPanel]: @ref nx::client::desktop::EventPanel
[EventRibbon]: @ref nx::client::desktop::EventRibbon
[EventTile]: @ref nx::client::desktop::EventTile
[SystemHealthListModel]: @ref nx::client::desktop::SystemHealthListModel
[ProgressListModel]: @ref nx::client::desktop::ProgressListModel
[NotificationListModel]: @ref nx::client::desktop::NotificationListModel
[ConcatenationListModel]: @ref nx::client::desktop::ConcatenationListModel
[MotionSearchListModel]: @ref nx::client::desktop::MotionSearchListModel
[BookmarkSearchListModel]: @ref nx::client::desktop::BookmarkSearchListModel
[EventSearchListModel]: @ref nx::client::desktop::EventSearchListModel
[AnalyticsSearchListModel]: @ref nx::client::desktop::AnalyticsSearchListModel

Right Panel is a pane at the right side of viewport that displays realtime notifications and 
system events of different kind, as well as provides means to lookup past events in the archive. 
Archive lookup is implemented as fetch-on-demand, loading data in batches when more data 
is requested.

The panel consists of several tabs, each of them displaying vertically scrollable ribbon of tiles.
The panel is represented by [EventPanel] class, the ribbon by [EventRibbon] class, and a tile
by [EventTile] class.

The ribbon is an item view displaying data from a list item model derived from QAbstractListModel.

@subpage right_panel_models

@subpage right_panel_widgets

## Notifications tab
Displays system health informers, progress informers and system notifications.
It uses a concatenation of [SystemHealthListModel], [ProgressListModel] and 
[NotificationListModel]. Concatenation is done via utility [ConcatenationListModel].

## Motion tab
Complements motion search mode and displays tiles corresponding to motion chunks. 
Uses [MotionSearchListModel].

## Bookmarks tab
Displays bookmarks. Uses [BookmarkSearchListModel].

## Events tab
Displays events recorded to system event log. Uses [EventSearchListModel].

## Detected objects tab
Displays object detection events recorded to the archive as well as those events happening 
in realtime. Uses [AnalyticsSearchListModel].

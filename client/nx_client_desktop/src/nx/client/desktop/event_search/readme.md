# Right Panel architecture {#right_panel}

Right Panel is a pane at the right side of viewport that displays realtime notifications and 
system events of different kind, as well as provides means to lookup past events in the archive. 
Archive lookup is implemented as fetch-on-demand, loading data in batches when more data 
is requested.

The panel consists of several tabs, each of them displaying vertically scrollable ribbon of tiles.
The panel is represented by **EventPanel** class, the ribbon by **EventRibbon** class, and a tile
by **EventTile** class.

The ribbon is an item view displaying data from a list item model derived from QAbstractListModel.

## Notifications tab
Displays system health informers, progress informers and system notifications.
It uses a concatenation of **SystemHealthListModel**, **ProgressListModel** and 
**NotificationListModel** (concatenation is done via utility **ConcatenationListModel**).

## Motion tab
Complements motion search mode and displays tiles corresponding to motion chunks. 
Uses **MotionSearchListModel**.

## Bookmarks tab

Displays bookmarks. Uses **BookmarkSearchListModel**.

## Events tab

Displays events recorded to system event log. Uses **EventSearchListModel**.

## Detected objects tab

Displays object detection events recorded to the archive as well as those events happening 
in realtime. Uses **AnalyticsSearchListModel**.

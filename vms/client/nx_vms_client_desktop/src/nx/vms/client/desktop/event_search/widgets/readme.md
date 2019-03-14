# Right Panel widget hierarchy {#right_panel_widgets}

[EventRibbon]: @ref nx::vms::client::desktop::EventRibbon
[SystemHealthListModel]: @ref nx::vms::client::desktop::SystemHealthListModel
[ProgressListModel]: @ref nx::vms::client::desktop::ProgressListModel
[NotificationListModel]: @ref nx::vms::client::desktop::NotificationListModel
[ConcatenationListModel]: @ref nx::vms::client::desktop::ConcatenationListModel
[AbstractSearchListModel]: @ref nx::vms::client::desktop::ProgressListModel
[SimpleMotionSearchListModel]: @ref nx::vms::client::desktop::MotionSearchListModel
[BookmarkSearchListModel]: @ref nx::vms::client::desktop::BookmarkSearchListModel
[EventSearchListModel]: @ref nx::vms::client::desktop::EventSearchListModel
[AnalyticsSearchListModel]: @ref nx::vms::client::desktop::AnalyticsSearchListModel
[MotionSearchSynchronizer]: @ref nx::vms::client::desktop::MotionSearchSynchronizer
[BookmarkSearchSynchronizer]: @ref nx::vms::client::desktop::BookmarkSearchSynchronizer
[AnalyticsSearchSynchronizer]: @ref nx::vms::client::desktop::AnalyticsSearchSynchronizer
[TileInteractionHandler](@ref nx::vms::client::desktop::TileInteractionHandler)

### nx::vms::client::desktop::EventPanel
The top-level panel widget.

- Contains tab control ([AnimatedTabWidget](@ref nx::vms::client::desktop::AnimatedTabWidget)).
- Synchronizes tab selection with **Workbench** state with the
help of [Right Panel Workbench synchronizers](@ref right_panel_synchronizers).
- Sustains unread notifications counter above Notifications tab title.
- Controls whole-panel context menu.
- Emits [tileHovered](@ref nx::vms::client::desktop::EventPanel::tileHovered) signals to allow
displaying complex tooltips in the graphics scene.

Each tab is a special widget containing [EventRibbon] and handling all interaction with it.

### nx::vms::client::desktop::NotificationListWidget
Notifications tab widget. Displays system health informers, progress informers and notifications
by concatenating (see [ConcatenationListModel]) owned item models ([SystemHealthListModel],
[ProgressListModel] and [NotificationListModel]) and passing the resulting model to own 
[EventRibbon]. Displays "No new notifications" placeholder if the resulting model is empty.

### nx::vms::client::desktop::AbstractSearchWidget
A base for *search* widgets. Implements all interaction between [AbstractSearchListModel] and
[EventRibbon] - fetch requests, fetch direction switches, live/non-live mode switches etc. 
Contains typical UI items that can be shown or hidden in descendants: search filters (camera
selector, time selector, full text search input) and show/hide buttons (previews toggle and
footers toggle). Displays placeholder if the model is empty (placeholder text and icon are
provided by descendants).

### nx::vms::client::desktop::SimpleMotionSearchWidget
Motion tab widget. Owns [SimpleMotionSearchListModel]. Supports only single-camera filter. 
Allows to programmatically set a region in which Motion Search is performed.
This widget is syncronized with **Workbench** state by [MotionSearchSynchronizer].

### nx::vms::client::desktop::BookmarkSearchWidget
Bookmarks tab widget. Owns [BookmarkSearchListModel]. Periodically polls for remote bookmark 
updates by calling [BookmarkSearchListModel::dynamicUpdate]
(@ref nx::vms::client::desktop::BookmarkSearchListModel::dynamicUpdate) method for the time range
currently in view. This widget is syncronized with **Workbench** state by 
[BookmarkSearchSynchronizer].

### nx::vms::client::desktop::EventSearchWidget
Events tab widget. Owns [EventSearchListModel]. Implements additional filter by event type.

### nx::vms::client::desktop::AnalyticsSearchWidget
Analytics tab widget. Owns [AnalyticsSearchListModel]. Implements additional filters by 
analytics object type and selected area. This widget is syncronized with **Workbench** state by
[AnalyticsSearchSynchronizer].

### nx::vms::client::desktop::EventRibbon
An item view displaying a scrollable vertical list of 
[EventTile](@ref nx::vms::client::desktop::EventTile) tiles. Gets data to display from a list 
item model. Emits a variety of signals to notify about tile mouse interaction, which is handled 
externally by [TileInteractionHandler].

Data roles used:
- Qt::DisplayRole - tile caption; if empty, the tile is considered a separator
- Qt::ForegroundRole - tile caption color
- Qt::DecorationRole - tile icon
- Qt::ToolTipRole - tile tooltip text
- Qn::DescriptionRole - tile description text
- Qn::BusyIndicatorVisibleRole - whether the tile is a busy indicator
- Qn::ProgressValueRole - current progress value and a sign that the tile is a progress 
information tile
- Qn::AlternateColorRole - whether the tile has *informer* color scheme
- Qn::TimestampTextRole - tile timestamp
- Qn::DurationRole - tile duration (used in currently played tiles highlighting)
- Qn::AdditionalTextRole - tile footer text
- Qn::RemovableRole - whether the tile should have a close button
- Qn::TimeoutRole - timeout after which the tile is automatically closed (must be removable)
- Qn::NotificationLevelRole - tile importance
- Qn::CommandActionRole - an action for tile action button
- Qn::HelpTopicIdRole - a help topic associated with the tile
- Qn::DisplayedResourceListRole - a list of resources or resource names to display in the tile
- Qn::ResourceListRole - a list of resources associated with the tile (used in currently played
tiles highlighting, also this role is used in [TileInteractionHandler])
- Qn::ResourceRole - a resource to load preview image from
- Qn::PreviewTimeRole - preview timestamp (live, if not specified)
- Qn::ItemZoomRectRole - preview highlight area (a sub-rectangle)
- Qn::ForcePrecisePreviewRole - whether precise preview should be requested (closest key frame
otherwise)
- Qn::PreviewStreamSelectionRole - preview stream selection mode
- Qn::ContextMenuRole - tile context menu (**TODO** Refactor this...)

### nx::vms::client::desktop::EventTile
A tile representing a notification, bookmark, event etc. 

Can be one of four types:
- informational tile
- progress information tile
- busy indicator tile
- separator tile

**Busy indicator tiles** are special 3-dot preloader tiles displayed at the top or bottom of
the view when more data is being asynchronously fetched by an item model (see
[AbstractAsyncSearchListModel](@ref nx::vms::client::desktop::AbstractAsyncSearchListModel)).

**Informational tiles** have switcheable *narrow* or *wide* layout. Can have special captionless mode
(used in Motion tab). Can have *standard* or *informer* visual styles (color schemes). 
Consist of several fields (all except caption are optional):
- caption
- icon
- timestamp
- close button
- resource list
- description text
- image preview
- footer text
- action button


# Right Panel Workbench synchronizers {#right_panel_synchronizers}
2-way synchronize current Right Panel tab selection and state with current **Workbench** state.

[EventPanel]: @ref nx::vms::client::desktop::EventPanel
[BookmarkSearchWidget]: @ref nx::vms::client::desktop::BookmarkSearchWidget
[SimpleMotionSearchWidget]: @ref nx::vms::client::desktop::SimpleMotionSearchWidget

### nx::vms::client::desktop::AbstractSearchSynchronizer
Abstract class for all Right Panel Workbench synchronizers.

Maintains *active* state which can be changed either by [EventPanel] when a tab associated with
the synchronizer becomes current or not, or by **Workbench** state changes (as descendants
connect to those **Workbench** signals that are relevant for them).

Also keeps track of **Workbench** resource widget currently selected on the scene.

### nx::vms::client::desktop::MotionSearchSynchronizer
2-way synchronizes its *active* state with *Motion Search* (*Smart Search*) mode of all cameras on
the current layout (and, indirectly, with Motion chunks display on the timeline). Synchronizes
motion search region in [SimpleMotionSearchWidget] with motion search region set for current
**Workbench** resource widget.

### nx::vms::client::desktop::BookmarkSearchSynchronizer
2-way synchronizes its *active* state with **BookmarksModeAction** toggle state (and, indirectly,
with bookmarks display on the timeline). Updates QnTimelineBookmarksWatcher text filter when
[BookmarkSearchWidget] text filter changes.

### nx::vms::client::desktop::AnalyticsSearchSynchronizer
Synchronizes Analytics chunks display with its *active* state. Synchronizes analytics search
rectangle in [AnalyticsSearchWidget] with the analytics selection rectangle on current 
**Workbench** resource widget.

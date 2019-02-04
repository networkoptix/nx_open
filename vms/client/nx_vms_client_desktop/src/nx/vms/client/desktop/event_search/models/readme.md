# Right Panel item models {#right_panel_models}
All data models are context-aware classes that connect to signals of various client components
and update data accordingly.

[SystemHealthListModel]: @ref nx::vms::client::desktop::SystemHealthListModel
[ProgressListModel]: @ref nx::vms::client::desktop::ProgressListModel
[EventListModel]: @ref nx::vms::client::desktop::EventListModel
[EventListModel::EventData]: @ref nx::vms::client::desktop::EventListModel::EventData
[NotificationListModel]: @ref nx::vms::client::desktop::NotificationListModel
[ConcatenationListModel]: @ref nx::vms::client::desktop::ConcatenationListModel
[MotionSearchListModel]: @ref nx::vms::client::desktop::MotionSearchListModel
[BookmarkSearchListModel]: @ref nx::vms::client::desktop::BookmarkSearchListModel
[EventSearchListModel]: @ref nx::vms::client::desktop::EventSearchListModel
[AnalyticsSearchListModel]: @ref nx::vms::client::desktop::AnalyticsSearchListModel
[WorkbenchProgressManager]: @ref nx::vms::client::desktop::WorkbenchProgressManager

## Abstract models

### nx::vms::client::desktop::AbstractEventListModel
Base type for all Right Panel item models.
Defines an interface for user-to-model notifications as virtual protected overridables
[defaultAction](@ref nx::vms::client::desktop::AbstractEventListModel::defaultAction) and 
[activateLink](@ref nx::vms::client::desktop::AbstractEventListModel::activateLink) called from 
[setData](@ref nx::vms::client::desktop::AbstractEventListModel::setData) with 
**Qn::DefaultNotificationRole** and **Qn::ActivateLinkRole** correspondingly.

### nx::vms::client::desktop::AbstractSearchListModel
Defines an interface for extended fetch-on-demand mechanic. The model maintains a time window
to underlying data. Maximum window size in item count can be specified. Either older or newer
data can be fetched depending on specified fetch direction. When more data is fetched and
maximum window size is exceeded, some data on the opposite side of the window is discarded
to comply with the size limitation. Relevant time period can be specified to constrain what data
can be fetched.

### nx::vms::client::desktop::AbstractAsyncSearchListModel
Defines interface and implements core mechanics of asynchronous fetch-on-demand.
Descendants must implement a set of virtual functions in a PIMPL derived from
[AbstractAsyncSearchListModel::Private](@ref nx::vms::client::desktop::AbstractAsyncSearchListModel::Private).

## Models without fetch-on-demand

### nx::vms::client::desktop::SystemHealthListModel
Maintains information about current system health events. Gets data and receives realtime updates
from **QnWorkbenchNotificationsHandler**.

### nx::vms::client::desktop::ProgressListModel
Provides information about asynchronous operations in the client that should display a progress
indicator. It is basically an item model interface to [WorkbenchProgressManager].
It keeps only a set of UUIDs of currently running asynchronous operations,
all detailed information about them is fetched from the progress manager.

### nx::vms::client::desktop::NotificationListModel
Maintains information about system notifications, inherits [EventListModel] class that
will be refactored in the future (most probably consumed by [NotificationListModel] itself).
Holds a list of [EventListModel::EventData] structures describing an event for which
system notification was received. Receives notifications from **QnWorkbenchNotificationsHandler**.

## Model with synchronous fetch-on-demand

### nx::vms::client::desktop::SimpleMotionSearchListModel
Maintains a window into already loaded motion chunks for currently selected camera.

## Models with asynchronous fetch-on-demand

### nx::vms::client::desktop::BookmarkSearchListModel
Performs bookmark search via ***/ec2/bookmarks*** request. Allows to apply specified text filter.
Listens to **QnCameraBookmarksManager** for realtime updates. Since there's no system-wide 
notifications about remote bookmark changes, has 
[dynamicUpdate](@ref nx::vms::client::desktop::BookmarkSearchListModel::dynamicUpdate) method
which should be called periodically to reload bookmarks within specified time period.

### nx::vms::client::desktop::EventSearchListModel
Performs event log lookup via ***/ec2/getEvents*** request. Allows to apply specified event type
filter. To emulate realtime updates periodically sends extra requests to fetch newly happening 
events.

### nx::vms::client::desktop::AnalyticsSearchListModel
Performs analytics lookup via ***/ec2/analyticsLookupDetectedObjects*** request. Allows to apply
specified text filter and specified rectangular area filter. Realtime updates are implemented by 
opening extra RTSP stream with metadata but without video data, with the help of
[LiveAnalyticsReceiver](@ref nx::vms::client::desktop::LiveAnalyticsReceiver) utility class.

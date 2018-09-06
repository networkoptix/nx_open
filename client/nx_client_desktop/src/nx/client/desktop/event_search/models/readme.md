# Right Panel item models {#right_panel_models}
All data models are context-aware classes that connect to signals of various client components
and update data accordingly.

## Abstract models

### AbstractEventListModel
Base type for all Right Panel item models.
Defines an interface for user-to-model notifications as virtual protected overridables
*defaultAction* and *activateLink* called from *setData* with **Qn::DefaultNotificationRole**
and **Qn::ActivateLinkRole** correspondingly.

### AbstractSearchListModel
Defines an interface for extended fetch-on-demand mechanic. The model maintains a time window
to underlying data. Maximum window size in item count can be specified. Either older or newer
data can be fetched depending on specified fetch direction. When more data is fetched and
maximum window size is exceeded, some data on the opposite side of the window is discarded
to comply with the size limitation. Relevant time period can be specified to constrain what data
can be fetched.

### AbstractAsyncSearchListModel
Defines interface and implements core mechanics of asynchronous fetch-on-demand.
Descendants must implement a set of virtual functions in a PIMPL derived from
**AbstractAsyncSearchListModel::Private** (*clearData*, *requestPrefetch*, *commitPrefetch*,
*hasAccessRights*, *truncateToMaximumCount*, *truncateToRelevantTimePeriod*, *count* and *data*).

## Models without fetch-on-demand

### SystemHealthListModel
Maintains information about current system health events. Gets data and receives realtime updates
from **WorkbenchNotificationsHandler**.

### ProgressListModel
Provides information about asynchronous operations in the client that should display a progress
indicator. It is basically an item model interface to **WorkbenchProgressManager**.
It keeps only a set of UUIDs of currently running asynchronous operations,
all detailed information about them is fetched from the progress manager.

### NotificationListModel
Maintains information about system notifications, inherits **EventListModel** class that
will be refactored in the future (most probably consumed by **NotificationListModel** itself).
Hold a list of **EventListModel::EventData** structures describing an event for which
system notification was received. Receives notifications from **WorkbenchNotificationsHandler**.

## Model with synchronous fetch-on-demand

### MotionSearchListModel
Maintains a window into already loaded motion chunks for currently selected camera.

*TODO: When multi-camera capability is added for Right Panel, this model will request chunks 
on its own and therefore will have asynchronous fetch-on-demand. Realtime updates will be 
implemented by opening extra RTSP stream with metadata but without video data.*

## Models with asynchronous fetch-on-demand

### BookmarkSearchListModel
Performs bookmark search via ***/ec2/bookmarks*** request. Allows to apply specified text filter.
Listens to **CameraBookmarksManager** for realtime updates. Informs **TimelineBookmarksWatcher**
about text filter currently set.

### EventSearchListModel
Performs event log lookup via ***/ec2/getEvents*** request. Allows to apply specified event type
filter. To emulate realtime updates periodically sends extra requests to fetch newly happening 
events.

### AnalyticsSearchListModel
Performs analytics lookup via ***/ec2/analyticsLookupDetectedObjects*** request. Allows to apply
specified text filter and specified rectangular area filter. 

*TODO: Realtime updates will be implemented by opening extra RTSP stream with metadata 
but without video data. Question: how to merge realtime data and fetched data seamlessly?*

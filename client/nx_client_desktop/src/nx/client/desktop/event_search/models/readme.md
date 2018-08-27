# Right Panel item models {#right_panel_models}
All data models are context-aware classes that connect to signals of various client components and update data accordingly.

## ToDo
*#vkutin* This file is not finished yet.

## Abstract models

### AbstractEventListModel
Base type for all Right Panel item models.
Defines an interface for extended fetch-on-demand mechanic.
Defines an interface for user-to-model notifications as virtual protected overridables *defaultAction* and *activateLink* called from *setData* with **Qn::DefaultNotificationRole** and **Qn::ActivateLinkRole** correspondingly.

### AbstractAsyncSearchListModel
Defines interface and implements core mechanics of asynchronous fetch-on-demand. Descendants must implement a set of virtual functions in a PIMPL derived from **AbstractAsyncSearchListModel::Private** (*requestPrefetch*, *commitPrefetch*, *clipToSelectedTimePeriod*, *hasAccessRights*, *count* and *data*). 

## Models without fetch-on-demand

### SystemHealthListModel
Maintains information about current system health events as a list of pairs of **QnSystemHealth::MessageType** and associated **QnResourcePtr**. Gets data and receives change notifications from **QnWorkbenchNotificationsHandler**.

### ProgressListModel
Provides information about asynchronous operations in the client that should display a progress indicator. It is basically an item model interface to **WorkbenchProgressManager**. It keeps only a set of UUIDs of currently running asynchronous operations, all detailed information about them is fetched from the progress manager.

### NotificationListModel
Maintains information about system notifications, inherits **EventListModel** class that will be refactored in the future (most probably consumed by **NotificationListModel** itself). Hold a list of **EventListModel::EventData** structures describing an event for which system notification was received. Receives system notifications from **QnWorkbenchNotificationsHandler**.

## Model with synchronous fetch-on-demand

### MotionSearchListModel

## Models with asynchronous fetch-on-demand

### BookmarkSearchListModel

### EventSearchListModel

### AnalyticsSearchListModel


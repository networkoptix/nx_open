// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_id.h"

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::rules;

using HelpIdMap = QMap<QString, int>;

namespace {

template <class T>
void addHelpId(HelpIdMap& map, int id)
{
    auto type = rules::utils::type<T>();
    NX_ASSERT(!map.contains(type));
    map.insert(std::move(type), id);
}

HelpIdMap eventHelpMap()
{
    HelpIdMap result;

    addHelpId<AnalyticsEvent>(result, HelpTopic::Id::EventsActions_VideoAnalytics);
    addHelpId<AnalyticsObjectEvent>(result, HelpTopic::Id::EventsActions_VideoAnalytics);
    addHelpId<BackupFinishedEvent>(result, HelpTopic::Id::EventsActions_BackupFinished);
    addHelpId<CameraInputEvent>(result, HelpTopic::Id::EventsActions_CameraInput);
    addHelpId<DeviceDisconnectedEvent>(result,
        HelpTopic::Id::EventsActions_CameraDisconnected);
    addHelpId<DeviceIpConflictEvent>(result, HelpTopic::Id::EventsActions_CameraIpConflict);
    addHelpId<GenericEvent>(result, HelpTopic::Id::EventsActions_Generic);
    addHelpId<LicenseIssueEvent>(result, HelpTopic::Id::EventsActions_LicenseIssue);
    addHelpId<MotionEvent>(result, HelpTopic::Id::EventsActions_CameraMotion);
    addHelpId<NetworkIssueEvent>(result, HelpTopic::Id::EventsActions_NetworkIssue);
    addHelpId<ServerConflictEvent>(result, HelpTopic::Id::EventsActions_MediaServerConflict);
    addHelpId<ServerFailureEvent>(result, HelpTopic::Id::EventsActions_MediaServerFailure);
    addHelpId<ServerStartedEvent>(result, HelpTopic::Id::EventsActions_MediaServerStarted);
    addHelpId<SoftTriggerEvent>(result, HelpTopic::Id::EventsActions_SoftTrigger);
    addHelpId<StorageIssueEvent>(result, HelpTopic::Id::EventsActions_StorageFailure);

    return result;
}

HelpIdMap actionHelpMap()
{
    HelpIdMap result;

    // addHelpId<Action>(result, HelpTopic::Id::EventsActions_ShowIntercomInformer);
    addHelpId<BookmarkAction>(result, HelpTopic::Id::EventsActions_Bookmark);
    addHelpId<DeviceOutputAction>(result, HelpTopic::Id::EventsActions_CameraOutput);
    addHelpId<DeviceRecordingAction>(result, HelpTopic::Id::EventsActions_StartRecording);
    addHelpId<HttpAction>(result, HelpTopic::Id::EventsActions_ExecHttpRequest);
    addHelpId<NotificationAction>(result,
        HelpTopic::Id::EventsActions_ShowDesktopNotification);
    addHelpId<PanicRecordingAction>(result, HelpTopic::Id::EventsActions_StartPanicRecording);
    addHelpId<PlaySoundAction>(result, HelpTopic::Id::EventsActions_PlaySound);
    addHelpId<PtzPresetAction>(result, HelpTopic::Id::EventsActions_ExecutePtzPreset);
    addHelpId<PushNotificationAction>(result,
        HelpTopic::Id::EventsActions_SendMobileNotification);
    addHelpId<RepeatSoundAction>(result, HelpTopic::Id::EventsActions_PlaySoundRepeated);
    addHelpId<SendEmailAction>(result, HelpTopic::Id::EventsActions_SendMail);
    addHelpId<ShowOnAlarmLayoutAction>(result,
        HelpTopic::Id::EventsActions_ShowOnAlarmLayout);
    addHelpId<SpeakAction>(result, HelpTopic::Id::EventsActions_Speech);
    addHelpId<TextOverlayAction>(result, HelpTopic::Id::EventsActions_ShowTextOverlay);
    addHelpId<WriteToLogAction>(result, HelpTopic::Id::EventsActions_Diagnostics);

    return result;
}

} // namespace

int eventHelpId(const QString& type)
{
    static const auto map = eventHelpMap();
    return map.value(type, -1);
}

int actionHelpId(const QString& type)
{
    static const auto map = actionHelpMap();
    return map.value(type, -1);
}

} // namespace nx::vms::client::desktop

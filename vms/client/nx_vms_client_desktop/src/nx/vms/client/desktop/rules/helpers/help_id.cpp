// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_id.h"

#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/help/help_topics.h>

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

    addHelpId<AnalyticsEvent>(result, Qn::EventsActions_VideoAnalytics_Help);
    addHelpId<AnalyticsObjectEvent>(result, Qn::EventsActions_VideoAnalytics_Help);
    addHelpId<BackupFinishedEvent>(result, Qn::EventsActions_BackupFinished_Help);
    addHelpId<CameraInputEvent>(result, Qn::EventsActions_CameraInput_Help);
    addHelpId<DeviceDisconnectedEvent>(result, Qn::EventsActions_CameraDisconnected_Help);
    addHelpId<DeviceIpConflictEvent>(result, Qn::EventsActions_CameraIpConflict_Help);
    addHelpId<GenericEvent>(result, Qn::EventsActions_Generic_Help);
    addHelpId<LicenseIssueEvent>(result, Qn::EventsActions_LicenseIssue_Help);
    addHelpId<MotionEvent>(result, Qn::EventsActions_CameraMotion_Help);
    addHelpId<NetworkIssueEvent>(result, Qn::EventsActions_NetworkIssue_Help);
    addHelpId<ServerConflictEvent>(result, Qn::EventsActions_MediaServerConflict_Help);
    addHelpId<ServerFailureEvent>(result, Qn::EventsActions_MediaServerFailure_Help);
    addHelpId<ServerStartedEvent>(result, Qn::EventsActions_MediaServerStarted_Help);
    addHelpId<SoftTriggerEvent>(result, Qn::EventsActions_SoftTrigger_Help);
    addHelpId<StorageIssueEvent>(result, Qn::EventsActions_StorageFailure_Help);

    return result;
}

HelpIdMap actionHelpMap()
{
    HelpIdMap result;

    // addHelpId<Action>(result, Qn::EventsActions_ShowIntercomInformer_Help);
    addHelpId<BookmarkAction>(result, Qn::EventsActions_Bookmark_Help);
    addHelpId<DeviceOutputAction>(result, Qn::EventsActions_CameraOutput_Help);
    addHelpId<DeviceRecordingAction>(result, Qn::EventsActions_StartRecording_Help);
    addHelpId<HttpAction>(result, Qn::EventsActions_ExecHttpRequest_Help);
    addHelpId<NotificationAction>(result, Qn::EventsActions_ShowDesktopNotification_Help);
    addHelpId<PanicRecordingAction>(result, Qn::EventsActions_StartPanicRecording_Help);
    addHelpId<PlaySoundAction>(result, Qn::EventsActions_PlaySound_Help);
    addHelpId<PtzPresetAction>(result, Qn::EventsActions_ExecutePtzPreset_Help);
    addHelpId<PushNotificationAction>(result, Qn::EventsActions_SendMobileNotification_Help);
    addHelpId<RepeatSoundAction>(result, Qn::EventsActions_PlaySoundRepeated_Help);
    addHelpId<SendEmailAction>(result, Qn::EventsActions_SendMail_Help);
    addHelpId<ShowOnAlarmLayoutAction>(result, Qn::EventsActions_ShowOnAlarmLayout_Help);
    addHelpId<SpeakAction>(result, Qn::EventsActions_Speech_Help);
    addHelpId<TextOverlayAction>(result, Qn::EventsActions_ShowTextOverlay_Help);
    addHelpId<WriteToLogAction>(result, Qn::EventsActions_Diagnostics_Help);

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

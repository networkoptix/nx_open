// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "help_topics.h"

#include <nx/utils/log/assert.h>

QString relativeUrlForTopic(Qn::HelpTopic topic)
{
    switch (topic)
    {
        case Qn::HelpTopic::Empty_Help:
        case Qn::HelpTopic::Forced_Empty_Help:
            return QString();

        case Qn::HelpTopic::About_Help:
            return "collecting_additional_informat.html";
        case Qn::HelpTopic::Administration_General_CamerasList_Help:
            return "viewing_full_cameras_list.html";
        case Qn::HelpTopic::Administration_Help:
            return "system-wide_configurations.html";
        case Qn::HelpTopic::Administration_RoutingManagement_Help:
            return "configuring_routing_within_mul.html";
        case Qn::HelpTopic::Administration_TimeSynchronization_Help:
            return "configuring_time_synchronizati.html";
        case Qn::HelpTopic::Administration_Update_Help:
            return "upgrading_vms.html";
        case Qn::HelpTopic::AuditTrail_Help:
            return "viewing_users_actions_log_audi.html";
        case Qn::HelpTopic::Bookmarks_Editing_Help:
            return "creating_and_setting_up_bookma.html";
        case Qn::HelpTopic::Bookmarks_Search_Help:
            return "search_using_bookmarks.html";
        case Qn::HelpTopic::Bookmarks_Usage_Help:
            return "using_bookmarks.html";
        case Qn::HelpTopic::CameraDiagnostics_Help:
            return "diagnosing_offline_cameras.html";
        case Qn::HelpTopic::CameraList_Help:
            return "viewing_full_cameras_list.html";
        case Qn::HelpTopic::CameraReplacementDialog_Help:
            return "replacing-a-camera.html";
        case Qn::HelpTopic::CameraSettingsExpertPage_Help:
            return "working_around_device_issues_e.html";
        case Qn::HelpTopic::CameraSettingsRecordingPage_Help:
            return "editing_recording_schedule.html";
        case Qn::HelpTopic::CameraSettingsWebPage_Help:
            return "configuring_devices_using_thei.html";
        case Qn::HelpTopic::CameraSettings_AspectRatio_Help:
            return "setting_camera_aspect_ratio.html";
        case Qn::HelpTopic::CameraSettings_Dewarping_Help:
            return "setting_up_fish-eye_cameras.html";
        case Qn::HelpTopic::CameraSettings_Expert_DisableArchivePrimary_Help:
            return "disabling_recording_of_primary.html";
        case Qn::HelpTopic::CameraSettings_Expert_Rtp_Help:
            return "setting_up_camera_transport_pr.html";
        case Qn::HelpTopic::CameraSettings_Expert_SettingsControl_Help:
            return "preventing_vms_from_c.html";
        case Qn::HelpTopic::CameraSettings_Help:
            return "setting_up_cameras_and_devices.html";
        case Qn::HelpTopic::CameraSettings_Motion_Help:
            return "setting_up_motion_mask_and_motion_sensitivity_(adm.html";
        case Qn::HelpTopic::CameraSettings_Recording_ArchiveLength_Help:
            return "configuring_maximum_and_minimu.html";
        case Qn::HelpTopic::CameraSettings_Recording_Export_Help:
            return "copying_recording_schedule_fro.html";
        case Qn::HelpTopic::CameraSettings_Rotation_Help:
            return "setting_camera_orientation.html";
        case Qn::HelpTopic::CameraSettings_SecondStream_Help:
            return "dual_streaming.html";
        case Qn::HelpTopic::CameraSettings_Onvif_Help:
            return "using-the-specific-onvif-profi.html";
        case Qn::HelpTopic::CertificateValidation_Help:
            return "certificate-validation.html";
        case Qn::HelpTopic::CloudLayoutsIntroduction_help:
            return "adding-items-to-layout.html";
        case Qn::HelpTopic::CloudLayoutsIntroductionAssign_Help:
            return "assigning-layout-to-users-and-.html";
        case Qn::HelpTopic::ConnectToCamerasOverOnlyHttps_Help:
            return "to-connect-to-cameras-over-onl.html";
        case Qn::HelpTopic::EnableArchiveEncryption_Help:
            return "to-enable-archive-encryption.html";
        case Qn::HelpTopic::EnableEncryptedVideoTraffic_Help:
            return "to-enable-encrypted-video-traf.html";
        case Qn::HelpTopic::EventLog_Help:
            return "viewing_events_log.html";
        case Qn::HelpTopic::EventsActions_BackupFinished_Help:
            return "archive_backup_finished.html";
        case Qn::HelpTopic::EventsActions_Bookmark_Help:
            return "create_bookmark.html";
        case Qn::HelpTopic::EventsActions_CameraDisconnected_Help:
            return "camera_disconnection_malfuncti.html";
        case Qn::HelpTopic::CollectingLogs_Help:
            return "collecting_logs.html";
        case Qn::HelpTopic::EventsActions_CameraInput_Help:
            return "input_signal_on_camera.html";
        case Qn::HelpTopic::EventsActions_CameraIpConflict_Help:
            return "camera_ip_conflict.html";
        case Qn::HelpTopic::EventsActions_CameraMotion_Help:
            return "motion_on_camera.html";
        case Qn::HelpTopic::EventsActions_CameraOutput_Help:
            return "trigger_camera_output.html";
        case Qn::HelpTopic::EventsActions_Diagnostics_Help:
            return "write_to_log.html";
        case Qn::HelpTopic::EventsActions_EmailNotSet_Help:
            return "e-mail_is_not_set_for_users.html";
        case Qn::HelpTopic::EventsActions_EmailServerNotSet_Help:
            return "e-mail_server_is_not_configure.html";
        case Qn::HelpTopic::EventsActions_ExecHttpRequest_Help:
            return "perform_http_request.html";
        case Qn::HelpTopic::EventsActions_ExecutePtzPreset_Help:
            return "execute_ptz_preset.html";
        case Qn::HelpTopic::EventsActions_Generic_Help:
            return "generic_event.html";
        case Qn::HelpTopic::EventsActions_Help:
            return "configuring_events_and_actions.html";
        case Qn::HelpTopic::EventsActions_LicenseIssue_Help:
            return "license_issue.html";
        case Qn::HelpTopic::EventsActions_MediaServerConflict_Help:
            return "media_servers_conflict.html";
        case Qn::HelpTopic::EventsActions_MediaServerFailure_Help:
            return "media_server_failure.html";
        case Qn::HelpTopic::EventsActions_MediaServerStarted_Help:
            return "media_server_started.html";
        case Qn::HelpTopic::EventsActions_NetworkIssue_Help:
            return "network_issue.html";
        case Qn::HelpTopic::EventsActions_NoLicenses_Help:
            return "licenses_are_not_configured.html";
        case Qn::HelpTopic::EventsActions_PlaySoundRepeated_Help:
            return "repeat_sound.html";
        case Qn::HelpTopic::EventsActions_PlaySound_Help:
            return "play_sound.html";
        case Qn::HelpTopic::EventsActions_Schedule_Help:
            return "setting_up_schedule_for_tracki.html";
        case Qn::HelpTopic::EventsActions_SendMailError_Help:
            return "error_while_sending_e-mail.html";
        case Qn::HelpTopic::EventsActions_SendMail_Help:
            return "mail_notifications.html";
        case Qn::HelpTopic::EventsActions_SendMobileNotification_Help:
            return "notifications.html";
        case Qn::HelpTopic::EventsActions_ShowDesktopNotification_Help:
            return "notifications.html";
        case Qn::HelpTopic::EventsActions_ShowOnAlarmLayout_Help:
            return "showing_cameras_on_alarm_layou.html";
        case Qn::HelpTopic::EventsActions_ShowIntercomInformer_Help:
            return "working-with-intercoms.html";
        case Qn::HelpTopic::EventsActions_ShowTextOverlay_Help:
            return "display_text_on_cameras.html";
        case Qn::HelpTopic::EventsActions_SoftTrigger_Help:
            return "soft_triggers.html";
        case Qn::HelpTopic::EventsActions_Speech_Help:
            return "say_text.html";
        case Qn::HelpTopic::EventsActions_StartPanicRecording_Help:
            return "start_panic_recording.html";
        case Qn::HelpTopic::EventsActions_StartRecording_Help:
            return "start_recording_on_camera.html";
        case Qn::HelpTopic::EventsActions_StorageFailure_Help:
            return "storage_failure.html";
        case Qn::HelpTopic::EventsActions_StoragesMisconfigured_Help:
            return "storages_misconfiguration.html";
        case Qn::HelpTopic::EventsActions_VideoAnalytics_Help:
            return "analytics_event.html";
        case Qn::HelpTopic::Exporting_Help:
            return "exporting.html";
        case Qn::HelpTopic::ForceSecureConnections_Help:
            return "to-force-secure-connections.html";
        case Qn::HelpTopic::IOModules_Help:
            return "setting_up_i_o_modules.html";
        case Qn::HelpTopic::ImageEnhancement_Help:
            return "color_correction.html";
        case Qn::HelpTopic::LaunchingAndClosing_Help:
            return "launching_and_closing_vms_client.html";
        case Qn::HelpTopic::LayoutSettings_EMapping_Help:
            return "e-mapping.html";
        case Qn::HelpTopic::LayoutSettings_Locking_Help:
            return "locking_layouts.html";
        case Qn::HelpTopic::Ldap_Help:
            return "adding_users_from_ldap_server.html";
        case Qn::HelpTopic::Licenses_Help:
            return "obtaining_and_activating_vms_licenses.html";
        case Qn::HelpTopic::Login_Help:
            return "connecting_to_enterprise_contr.html";
        case Qn::HelpTopic::MainWindow_Calendar_Help:
            return "using_calendar.html";
        case Qn::HelpTopic::MainWindow_ContextHelp_Help:
            return "getting_context_help.html";
        case Qn::HelpTopic::MainWindow_DayTimePicker_Help:
            return "using_calendar.html";
        case Qn::HelpTopic::MainWindow_Fullscreen_Help:
            return "full_screen_and_windowed_mode.html";
        case Qn::HelpTopic::MainWindow_MediaItem_AnalogCamera_Help:
            return "setting_up_analog_cameras.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Dewarping_Help:
            return "working_with_fish-eye_cameras.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Diagnostics_Help:
            return "diagnosing_offline_cameras.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Help:
            return "viewing_camera_stream_information.html";
        case Qn::HelpTopic::MainWindow_MediaItem_ImageEnhancement_Help:
            return "color_correction.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Local_Help:
            return "playing_back_local_files_in_vms.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Ptz_Help:
            return "setting_up_ptz.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Rotate_Help:
            return "rotate.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Screenshot_Help:
            return "taking_a_screenshot.html";
        case Qn::HelpTopic::MainWindow_MediaItem_SmartSearch_Help:
            return "performing_smart_search.html";
        case Qn::HelpTopic::MainWindow_MediaItem_Unauthorized_Help:
            return "configuring_camera_authentication_(admin_only).html";
        case Qn::HelpTopic::MainWindow_MediaItem_AccessDenied_Help:
            return "configuring_camera_authentication_(admin_only).html";
        case Qn::HelpTopic::MainWindow_MediaItem_ZoomWindows_Help:
            return "zoom_windows.html";
        case Qn::HelpTopic::MainWindow_MonitoringItem_Help:
            return "monitoring_media_servers.html";
        case Qn::HelpTopic::MainWindow_MonitoringItem_Log_Help:
            return "viewing_events_log.html";
        case Qn::HelpTopic::MainWindow_Navigation_Help:
            return "navigating_through_archive_and.html";
        case Qn::HelpTopic::MainWindow_Notifications_EventLog_Help:
            return "viewing_events_log.html";
        case Qn::HelpTopic::MainWindow_Pin_Help:
            return "showing_and_hiding_side_panels.html";
        case Qn::HelpTopic::MainWindow_Playback_Help:
            return "playback_panel.html";
        case Qn::HelpTopic::MainWindow_Scene_EMapping_Help:
            return "e-mapping.html";
        case Qn::HelpTopic::MainWindow_Scene_Help:
            return "index.html";
        case Qn::HelpTopic::MainWindow_Scene_PreviewSearch_Help:
            return "thumbnail_search.html";
        case Qn::HelpTopic::MainWindow_Scene_TourInProgress_Help:
            return "tours.html";
        case Qn::HelpTopic::MainWindow_Slider_Timeline_Help:
            return "pan-temporal_timeline.html";
        case Qn::HelpTopic::MainWindow_Slider_Volume_Help:
            return "adjusting_volume_while_viewing_local_files.html";
        case Qn::HelpTopic::MainWindow_Sync_Help:
            return "navigating_through_several_cameras_synchronously.html";
        case Qn::HelpTopic::MainWindow_Thumbnails_Help:
            return "using_thumbnails_for_better_navigation.html";
        case Qn::HelpTopic::MainWindow_TitleBar_Cloud_Help:
            return "connecting_as_cloud_user.html";
        case Qn::HelpTopic::MainWindow_TitleBar_MainMenu_Help:
            return "main_menu.html";
        case Qn::HelpTopic::MainWindow_TitleBar_NewLayout_Help:
            return "creating_a_new_layout_(admin_only).html";
        case Qn::HelpTopic::MainWindow_Tree_Camera_Help:
            return "configuring_cameras.html";
        case Qn::HelpTopic::MainWindow_Tree_Exported_Help:
            return "viewing_videos_exported_from_a.html";
        case Qn::HelpTopic::MainWindow_Tree_Help:
            return "working_with_a_resource_tree.html";
        case Qn::HelpTopic::MainWindow_Tree_Layouts_Help:
            return "customizing_product_name.html";
        case Qn::HelpTopic::MainWindow_Tree_MultiVideo_Help:
            return "changing_multi-channel_video_a.html";
        case Qn::HelpTopic::MainWindow_Tree_Recorder_Help:
            return "setting_up_analog_cameras.html";
        case Qn::HelpTopic::MainWindow_Tree_Servers_Help:
            return "configuring_media_servers_additional_settings_(ad.html";
        case Qn::HelpTopic::MainWindow_Tree_Users_Help:
            return "users_management.html";
        case Qn::HelpTopic::MainWindow_Tree_WebPage_Help:
            return "using_display_product_name_as_.html";
        case Qn::HelpTopic::MainWindow_WebPageItem_Help:
            return "using_display_product_name_as_.html";
        case Qn::HelpTopic::MediaFolders_Help:
            return "configuring_media_folders.html";
        case Qn::HelpTopic::NewUser_Help:
            return "creating_a_new_user_(admin_only).html";
        case Qn::HelpTopic::NotificationsPanel_Help:
            return "notifications_panel.html";
        case Qn::HelpTopic::OtherSystems_Help:
            return "configuring_multi-server_envir.html";
        case Qn::HelpTopic::PluginsAndAnalytics_Help:
            return "plugins-and-analytics.html";
        case Qn::HelpTopic::PtzManagement_Tour_Help:
            return "setting_up_ptz_tours.html";
        case Qn::HelpTopic::PtzPresets_Help:
            return "saving_and_restoring_ptz_posit.html";
        case Qn::HelpTopic::Rapid_Review_Help:
            return "rapid_review_export.html";
        case Qn::HelpTopic::SaveLayout_Help:
            return "saving_layouts_(admin_only).html";
        case Qn::HelpTopic::SecureConnection_Help:
            return "configuring-secure-connections.html";
        case Qn::HelpTopic::ServerSettings_ArchiveRestoring_Help:
            return "restoring_broken_archive.html";
        case Qn::HelpTopic::ServerSettings_Failover_Help:
            return "configuring_failover.html";
        case Qn::HelpTopic::ServerSettings_General_Help:
            return "configuring_media_servers_additional_settings_(ad.html";
        case Qn::HelpTopic::ServerSettings_StorageAnalitycs_Help:
            return "analyzingandanticipatingstorageusage.html";
        case Qn::HelpTopic::ServerSettings_StoragesBackup_Help:
            return "configuring_redundant_storage_.html";
        case Qn::HelpTopic::ServerSettings_Storages_Help:
            return "configuring_media_server_storages_(admin_only).html";
        case Qn::HelpTopic::ServerSettings_WebClient_Help:
            return "using_media_servers_web_interf.html";
        case Qn::HelpTopic::SessionAndDigestAuth_Help:
            return "session-and-digest-authentication.html";
        case Qn::HelpTopic::Setup_Wizard_Help:
            return "vms_quick_start.html";
        case Qn::HelpTopic::Showreel_Help:
            return "showreel_layout_tours.html";
        case Qn::HelpTopic::SystemSettings_Cloud_Help:
            return "connecting_system_to_cloud.html";
        case Qn::HelpTopic::SystemSettings_General_AnonymousUsage_Help:
            return "sending_anonymous_usage_and_cr.html";
        case Qn::HelpTopic::SystemSettings_General_AutoPause_Help:
            return "cpu_and_bandwidth_saving_durin.html";
        case Qn::HelpTopic::SystemSettings_General_Customizing_Help:
            return "customizing_product_name.html";
        case Qn::HelpTopic::SystemSettings_General_DoubleBuffering_Help:
            return "double_buffering.html";
        case Qn::HelpTopic::SystemSettings_General_Logs_Help:
            return "collecting_logs.html";
        case Qn::HelpTopic::SystemSettings_General_ShowIpInTree_Help:
            return "working_with_a_resource_tree.html";
        case Qn::HelpTopic::SystemSettings_General_TourCycleTime_Help:
            return "tours.html";
        case Qn::HelpTopic::SystemSettings_Notifications_Help:
            return "notifications.html";
        case Qn::HelpTopic::SystemSettings_ScreenRecording_Help:
            return "setting_up_screen_recording.html";
        case Qn::HelpTopic::SystemSettings_Server_Backup_Help:
            return "backing_up_and_restoring_produ.html";
        case Qn::HelpTopic::SystemSettings_Server_CameraAutoDiscovery_Help:
            return "disabling_automatic_discovery.html";
        case Qn::HelpTopic::SystemSettings_Server_Mail_Help:
            return "configuring_mail_server_for_e-.html";
        case Qn::HelpTopic::SystemSettings_UserManagement_Help:
            return "user_management_form.html";
        case Qn::HelpTopic::Systems_ConnectToCurrentSystem_Help:
            return "join_server_from_different_sys.html";
        case Qn::HelpTopic::Systems_MergeSystems_Help:
            return "merge_all_servers_to_another_s.html";
        case Qn::HelpTopic::UserRoles_Help:
            return "roles_management.html";
        case Qn::HelpTopic::UserSettings_DisableUser_Help:
            return "disabling_user.html";
        case Qn::HelpTopic::UserSettings_Help:
            return "changing_user_settings.html";
        case Qn::HelpTopic::UserSettings_UserRoles_Help:
            return "introducing_user_roles.html";
        case Qn::HelpTopic::UserWatermark_Help:
            return "user_watermark_on_exports.html";
        case Qn::HelpTopic::UsingJoystick_Help:
            return "using-joysticks.html";
        case Qn::HelpTopic::VersionMismatch_Help:
            return "launching_product_name_in_comp.html";
        case Qn::HelpTopic::Videowall_Attach_Help:
            return "creating_new_video_wall.html";
        case Qn::HelpTopic::Videowall_Display_Help:
            return "controlling_video_wall_display.html";
        case Qn::HelpTopic::Videowall_Help:
            return "configuring_video_wall_several.html";
        case Qn::HelpTopic::Videowall_Management_Help:
            return "video_wall_management.html";
        case Qn::HelpTopic::VirtualCamera_Help:
            return "virtual_camera.html";
        case Qn::HelpTopic::Watermark_Help:
            return "viewing_and_checking_the_validity_of_exported_vide.html";

        default:
            NX_ASSERT(false, "Unhandled help topic %1", (int)topic);
            return QString();
    }
}

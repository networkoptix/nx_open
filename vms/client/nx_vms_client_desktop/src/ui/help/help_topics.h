// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace Qn {

enum HelpTopic
{
    Empty_Help = 0,
    Forced_Empty_Help,
    About_Help,
    Administration_General_CamerasList_Help,
    Administration_Help,
    Administration_RoutingManagement_Help,
    Administration_TimeSynchronization_Help,
    Administration_Update_Help,
    AuditTrail_Help,
    Bookmarks_Editing_Help,
    Bookmarks_Search_Help,
    Bookmarks_Usage_Help,
    CameraDiagnostics_Help,
    CameraList_Help,
    CameraSettingsExpertPage_Help,
    CameraSettingsRecordingPage_Help,
    CameraSettingsWebPage_Help,
    CameraSettings_AspectRatio_Help,
    CameraSettings_Dewarping_Help,
    CameraSettings_Expert_DisableArchivePrimary_Help,
    CameraSettings_Expert_Rtp_Help,
    CameraSettings_Expert_SettingsControl_Help,
    CameraSettings_Help,
    CameraSettings_Motion_Help,
    CameraSettings_Recording_ArchiveLength_Help,
    CameraSettings_Recording_Export_Help,
    CameraSettings_Rotation_Help,
    CameraSettings_SecondStream_Help,
    CertificateValidation_Help,
    ConnectToCamerasOverOnlyHttps_Help,
    EnableArchiveEncryption_Help,
    EnableEncryptedVideoTraffic_Help,
    EventLog_Help,
    EventsActions_BackupFinished_Help,
    EventsActions_Bookmark_Help,
    EventsActions_CameraDisconnected_Help,
    EventsActions_CameraInput_Help,
    EventsActions_CameraIpConflict_Help,
    EventsActions_CameraMotion_Help,
    EventsActions_CameraOutput_Help,
    EventsActions_Diagnostics_Help,
    EventsActions_EmailNotSet_Help,
    EventsActions_EmailServerNotSet_Help,
    EventsActions_ExecHttpRequest_Help,
    EventsActions_ExecutePtzPreset_Help,
    EventsActions_Generic_Help,
    EventsActions_Help,
    EventsActions_LicenseIssue_Help,
    EventsActions_MediaServerConflict_Help,
    EventsActions_MediaServerFailure_Help,
    EventsActions_MediaServerStarted_Help,
    EventsActions_NetworkIssue_Help,
    EventsActions_NoLicenses_Help,
    EventsActions_PlaySoundRepeated_Help,
    EventsActions_PlaySound_Help,
    EventsActions_Schedule_Help,
    EventsActions_SendMailError_Help,
    EventsActions_SendMail_Help,
    EventsActions_SendMobileNotification_Help,
    EventsActions_ShowDesktopNotification_Help,
    EventsActions_ShowOnAlarmLayout_Help,
    EventsActions_ShowIntercomInformer_Help,
    EventsActions_ShowTextOverlay_Help,
    EventsActions_SoftTrigger_Help,
    EventsActions_Speech_Help,
    EventsActions_StartPanicRecording_Help,
    EventsActions_StartRecording_Help,
    EventsActions_StorageFailure_Help,
    EventsActions_StoragesMisconfigured_Help,
    EventsActions_VideoAnalytics_Help,
    Exporting_Help,
    ForceSecureConnections_Help,
    IOModules_Help,
    ImageEnhancement_Help,
    LaunchingAndClosing_Help,
    LayoutSettings_EMapping_Help,
    LayoutSettings_Locking_Help,
    Ldap_Help,
    Licenses_Help,
    Login_Help,
    MainWindow_Calendar_Help,
    MainWindow_ContextHelp_Help,
    MainWindow_DayTimePicker_Help,
    MainWindow_Fullscreen_Help,
    MainWindow_MediaItem_AnalogCamera_Help,
    MainWindow_MediaItem_Dewarping_Help,
    MainWindow_MediaItem_Diagnostics_Help,
    MainWindow_MediaItem_Help,
    MainWindow_MediaItem_ImageEnhancement_Help,
    MainWindow_MediaItem_Local_Help,
    MainWindow_MediaItem_Ptz_Help,
    MainWindow_MediaItem_Rotate_Help,
    MainWindow_MediaItem_Screenshot_Help,
    MainWindow_MediaItem_SmartSearch_Help,
    MainWindow_MediaItem_Unauthorized_Help,
    MainWindow_MediaItem_AccessDenied_Help,
    MainWindow_MediaItem_ZoomWindows_Help,
    MainWindow_MonitoringItem_Help,
    MainWindow_MonitoringItem_Log_Help,
    MainWindow_Navigation_Help,
    MainWindow_Notifications_EventLog_Help,
    MainWindow_Pin_Help,
    MainWindow_Playback_Help,
    MainWindow_Scene_EMapping_Help,
    MainWindow_Scene_Help,
    MainWindow_Scene_PreviewSearch_Help,
    MainWindow_Scene_TourInProgress_Help,
    MainWindow_Slider_Timeline_Help,
    MainWindow_Slider_Volume_Help,
    MainWindow_Sync_Help,
    MainWindow_Thumbnails_Help,
    MainWindow_TitleBar_Cloud_Help,
    MainWindow_TitleBar_MainMenu_Help,
    MainWindow_TitleBar_NewLayout_Help,
    MainWindow_Tree_Camera_Help,
    MainWindow_Tree_Exported_Help,
    MainWindow_Tree_Help,
    MainWindow_Tree_Layouts_Help,
    MainWindow_Tree_MultiVideo_Help,
    MainWindow_Tree_Recorder_Help,
    MainWindow_Tree_Servers_Help,
    MainWindow_Tree_Users_Help,
    MainWindow_Tree_WebPage_Help,
    MainWindow_WebPageItem_Help,
    MediaFolders_Help,
    NewUser_Help,
    NotificationsPanel_Help,
    OtherSystems_Help,
    PluginsAndAnalytics_Help,
    PtzManagement_Tour_Help,
    PtzPresets_Help,
    Rapid_Review_Help,
    SaveLayout_Help,
    SecureConnection_Help,
    ServerSettings_ArchiveRestoring_Help,
    ServerSettings_Failover_Help,
    ServerSettings_General_Help,
    ServerSettings_StorageAnalitycs_Help,
    ServerSettings_StoragesBackup_Help,
    ServerSettings_Storages_Help,
    ServerSettings_WebClient_Help,
    SessionAndDigestAuth_Help,
    Setup_Wizard_Help,
    Showreel_Help,
    SystemSettings_Cloud_Help,
    SystemSettings_General_AnonymousUsage_Help,
    SystemSettings_General_AutoPause_Help,
    SystemSettings_General_Customizing_Help,
    SystemSettings_General_DoubleBuffering_Help,
    SystemSettings_General_Logs_Help,
    SystemSettings_General_ShowIpInTree_Help,
    SystemSettings_General_TourCycleTime_Help,
    SystemSettings_Notifications_Help,
    SystemSettings_ScreenRecording_Help,
    SystemSettings_Server_Backup_Help,
    SystemSettings_Server_CameraAutoDiscovery_Help,
    SystemSettings_Server_Mail_Help,
    SystemSettings_UserManagement_Help,
    Systems_ConnectToCurrentSystem_Help,
    Systems_MergeSystems_Help,
    UserRoles_Help,
    UserSettings_DisableUser_Help,
    UserSettings_Help,
    UserSettings_UserRoles_Help,
    UserWatermark_Help,
    VersionMismatch_Help,
    Videowall_Attach_Help,
    Videowall_Display_Help,
    Videowall_Help,
    Videowall_Management_Help,
    VirtualCamera_Help,
    Watermark_Help,

    HelpTopicCount
};

} // namespace Qn

QString relativeUrlForTopic(Qn::HelpTopic topic);

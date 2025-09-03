// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_meta_types.h"

#include <mutex>

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client_core/client_core_meta_types.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/cloud/db/api/result_code.h>
#include <nx/cloud/db/api/system_data.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/thumbnails/resource_id_thumbnail.h>
#include <nx/vms/client/desktop/access/screen_recording_watcher.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_filter.h>
#include <nx/vms/client/desktop/analytics/analytics_dialog_table_model.h>
#include <nx/vms/client/desktop/analytics/attribute_display_manager.h>
#include <nx/vms/client/desktop/analytics/attribute_visibility_settings_model.h>
#include <nx/vms/client/desktop/analytics/tableview_columns_calculator.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/models/index_list_model.h>
#include <nx/vms/client/desktop/common/models/row_check_model.h>
#include <nx/vms/client/desktop/common/models/sort_filter_proxy_model.h>
#include <nx/vms/client/desktop/common/utils/audio_dispatcher.h>
#include <nx/vms/client/desktop/common/utils/list_navigation_helper.h>
#include <nx/vms/client/desktop/common/utils/qml_client_state_selegate.h>
#include <nx/vms/client/desktop/common/utils/quick_message_box.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/debug_utils/components/performance_info.h>
#include <nx/vms/client/desktop/debug_utils/dialogs/joystick_investigation_wizard/joystick_manager.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>
#include <nx/vms/client/desktop/event_search/utils/call_alarm_manager.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/joystick/dialog/joystick_button_action_choice_model.h>
#include <nx/vms/client/desktop/joystick/dialog/joystick_button_settings_model.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_lists_dialog.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/resource_status_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/filtering/filtered_resource_proxy_model.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_model_adapter.h>
#include <nx/vms/client/desktop/resource_properties/camera/widgets/motion_regions_item.h>
#include <nx/vms/client/desktop/resource_properties/fisheye/fisheye_calibrator.h>
#include <nx/vms/client/desktop/resource_properties/schedule/record_schedule_cell_data.h>
#include <nx/vms/client/desktop/resource_properties/user/models/resource_access_rights_model.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_rights_list.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/limited_model.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/item_view_drag_and_drop_scroll_assist.h>
#include <nx/vms/client/desktop/rules/model_view/key_value_model.h>
#include <nx/vms/client/desktop/rules/model_view/rules_sort_filter_proxy_model.h>
#include <nx/vms/client/desktop/rules/model_view/rules_table_model.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_administration/dialogs/integrations_dialog.h>
#include <nx/vms/client/desktop/system_administration/globals/ldap_settings.h>
#include <nx/vms/client/desktop/system_administration/globals/user_settings_global.h>
#include <nx/vms/client/desktop/system_administration/models/global_permissions_model.h>
#include <nx/vms/client/desktop/system_administration/models/ldap_filters_model.h>
#include <nx/vms/client/desktop/system_administration/models/members_model.h>
#include <nx/vms/client/desktop/system_administration/models/recursive_members_model.h>
#include <nx/vms/client/desktop/system_administration/models/server_list_model.h>
#include <nx/vms/client/desktop/system_administration/models/time_duration_suffix_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/data/connect_tiles_proxy_model.h>
#include <nx/vms/client/desktop/system_logon/data/systems_visibility_sort_filter_model.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <nx/vms/client/desktop/thumbnails/live_camera_thumbnail.h>
#include <nx/vms/client/desktop/thumbnails/roi_camera_thumbnail.h>
#include <nx/vms/client/desktop/ui/common/context_help.h>
#include <nx/vms/client/desktop/ui/common/cursor_override.h>
#include <nx/vms/client/desktop/ui/common/custom_cursor.h>
#include <nx/vms/client/desktop/ui/common/custom_cursors.h>
#include <nx/vms/client/desktop/ui/common/drag_and_drop.h>
#include <nx/vms/client/desktop/ui/common/embedded_popup.h>
#include <nx/vms/client/desktop/ui/common/focus_frame_item.h>
#include <nx/vms/client/desktop/ui/common/global_tool_tip.h>
#include <nx/vms/client/desktop/ui/common/item_grabber.h>
#include <nx/vms/client/desktop/ui/common/keyboard_modifiers.h>
#include <nx/vms/client/desktop/ui/common/whats_this.h>
#include <nx/vms/client/desktop/ui/right_panel/models/right_panel_models_adapter.h>
#include <nx/vms/client/desktop/ui/scene/instruments/instrument.h>
#include <nx/vms/client/desktop/ui/scene/item_model_utils.h>
#include <nx/vms/client/desktop/ui/scene/models/layout_model.h>
#include <nx/vms/client/desktop/ui/scene/models/resource_tree_model_adapter.h>
#include <nx/vms/client/desktop/utils/blur_mask.h>
#include <nx/vms/client/desktop/utils/cursor_manager.h>
#include <nx/vms/client/desktop/utils/date_validator.h>
#include <nx/vms/client/desktop/utils/mouse_spy.h>
#include <nx/vms/client/desktop/utils/server_file_cache.h>
#include <nx/vms/client/desktop/utils/upload_state.h>
#include <nx/vms/client/desktop/utils/webengine_profile_manager.h>
#include <nx/vms/client/desktop/utils/window_context_aware_attached.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_payload.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_state.h>
#include <nx/vms/client/desktop/webpage/web_page_data_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/state/thumbnail_search_state.h>
#include <nx/vms/client/desktop/workbench/timeline/archive_frame_extractor.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail.h>
#include <nx/vms/client/desktop/workbench/timeline/timeline_globals.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_context.h>
#include <recording/time_period.h>
#include <ui/common/notification_levels.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/view/rhi_rendering_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/color_space/image_correction.h>
#include <utils/ping_utility.h>

Q_DECLARE_METATYPE(nx::cloud::db::api::ResultCode)
Q_DECLARE_METATYPE(nx::cloud::db::api::SystemData)
Q_DECLARE_METATYPE(rest::ServerConnectionPtr)

namespace nx::vms::client::desktop {

namespace {

QnResourcePtr stringToResource(const QString& s)
{
    const QStringList files{s};
    const auto acceptedFiles = QnFileProcessor::findAcceptedFiles(files);
    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
    if (acceptedFiles.empty())
    {
        return resourcePool->getResourceById(nx::Uuid::fromStringSafe(s));
    }
    const auto resources = QnFileProcessor::createResourcesForFiles(acceptedFiles, resourcePool);
    if (resources.empty())
        return QnResourcePtr();
    return resources.front();
}

QnResourceList stringToResourceList(const QString& s)
{
    const QStringList files{s};
    const auto acceptedFiles = QnFileProcessor::findAcceptedFiles(files);
    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
    if (acceptedFiles.empty())
    {
        const auto resource = resourcePool->getResourceById(nx::Uuid::fromStringSafe(s));
        if (!resource)
            return {};
        return {resource};
    }
    return QnFileProcessor::createResourcesForFiles(acceptedFiles, resourcePool);
}

QString resourceListToString(const QnResourceList& list)
{
    QStringList ids;
    for (const auto& r: list)
        ids << r->getId().toString(QUuid::WithBraces);
    return ids.join(',');
}

QString uuidToString(const nx::Uuid& uuid)
{
    return uuid.toString(QUuid::WithBraces);
}

} // namespace

void initializeMetatypesInternal()
{
    core::initializeMetaTypes();

    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<QVector<QColor> >();
    qRegisterMetaType<QValidator::State>();
    qRegisterMetaType<ResourceTree::NodeType>();
    qRegisterMetaType<Qn::ItemRole>();
    qRegisterMetaType<Qn::ItemDataRole>();
    qRegisterMetaType<workbench::timeline::ThumbnailPtr>();
    qRegisterMetaType<Qn::TimeMode>();
    qRegisterMetaType<Qn::ImageBehavior>();
    qRegisterMetaType<menu::IDType>("menu::IDType");
    qRegisterMetaType<menu::Parameters>();
    qRegisterMetaType<ThumbnailsSearchState>();
    qRegisterMetaType<Qn::LightModeFlags>();
    qRegisterMetaType<WeakGraphicsItemPointerList>();
    qRegisterMetaType<QnPingUtility::PingResponse>();
    qRegisterMetaType<ServerFileCache::OperationResult>();
    qRegisterMetaType<UploadState>();
    qRegisterMetaType<VirtualCameraState>();
    qRegisterMetaType<VirtualCameraUpload>();
    qRegisterMetaType<nx::cloud::db::api::ResultCode>();
    qRegisterMetaType<nx::cloud::db::api::SystemData>();
    qRegisterMetaType<rest::ServerConnectionPtr>();
    qRegisterMetaType<ExportMediaPersistentSettings>();
    qRegisterMetaType<nx::vms::event::Level>();
    qRegisterMetaType<nx::vms::common::update::Information>();
    qRegisterMetaType<UpdateDeliveryInfo>();
    qRegisterMetaType<UpdateContents>();
    qRegisterMetaType<ArchiveFrameExtractor::Result>();
    qRegisterMetaType<RecordScheduleCellData>();

    QMetaType::registerConverter<QVariantMap, QnTimePeriod>(variantMapToTimePeriod);
    QMetaType::registerConverter<QString, QnResourcePtr>(stringToResource);
    QMetaType::registerConverter<QString, QnResourceList>(stringToResourceList);
    QMetaType::registerConverter<QnResourceList, QString>(resourceListToString);
    QMetaType::registerConverter<nx::Uuid, QString>(uuidToString);
    QMetaType::registerConverter<QnCameraBookmarkList, QVariantList>(bookmarkListToVariantList);
    QMetaType::registerConverter<QVariantList, QnCameraBookmarkList>(variantListToBookmarkList);
    QMetaType::registerConverter<QnCameraBookmark, QString>(bookmarkToString);
    QnMessageBox::registerLogInfoHandler();

    WebViewController::registerMetaType();

    QnJsonSerializer::registerSerializer<Qn::ImageBehavior>();
}

void initializeMetaTypes()
{
    static std::once_flag initialized;
    std::call_once(initialized, &initializeMetatypesInternal);
}

void registerQmlTypesInternal()
{
    core::registerQmlTypes();

    SystemContext::registerQmlType();
    AccessRightsList::registerQmlTypes();
    LayoutModel::registerQmlType();
    RowCheckModel::registerQmlType();
    IndexListModel::registerQmlType();
    QmlClientStateDelegate::registerQmlType();
    RightPanelModelsAdapter::registerQmlTypes();
    ResourceSelectionModelAdapter::registerQmlType();
    ResourceTreeModelAdapter::registerQmlType();
    ResourceStatusHelper::registerQmlType();
    RightPanel::registerQmlType();
    ResourceTree::registerQmlType();
    ItemModelUtils::registerQmlType();
    ListNavigationHelper::registerQmlType();
    ItemGrabber::registerQmlType();
    DragAndDrop::registerQmlType();
    ResourceAccessRightsModel::registerQmlTypes();
    LimitedModel::registerQmlType();
    GlobalPermissionsModel::registerQmlTypes();
    LiveCameraThumbnail::registerQmlType();
    RoiCameraThumbnail::registerQmlType();
    QuickMessageBox::registerQmlType();
    EmbeddedPopup::registerQmlType();
    ProximityScrollHelper::registerQmlType();
    ContextHelp::registerQmlType();
    WhatsThis::registerQmlType();
    MouseSpyInterface::registerQmlType();
    AudioDispatcher::registerQmlType();
    UserSettingsGlobal::registerQmlTypes();
    MembersModel::registerQmlType();
    RecursiveMembersModel::registerQmlType();
    LdapFiltersModel::registerQmlType();
    ServerListModel::registerQmlType();
    LookupListsDialog::registerQmlTypes();
    joystick::JoystickManager::registerQmlType();
    TimeDurationSuffixModel::registerQmlType();
    WindowContextAwareHelper::registerQmlType();
    KeyboardModifiers::registerQmlType();
    DateValidator::registerQmlType();
    WindowContext::registerQmlType();
    CallAlarmManager::registerQmlType();
    IntegrationsDialog::registerQmlType();
    analytics::AttributeFilter::registerQmlType();
    analytics::taxonomy::AttributeVisibilitySettingsModel::registerQmlType();
    analytics::taxonomy::AttributeDisplayManager::registerQmlType();
    analytics::taxonomy::AnalyticsDialogTableModel::registerQmlType();
    TableViewColumnsCalculator::registerQmlType();

    qmlRegisterType<FisheyeCalibrator>("nx.vms.client.desktop", 1, 0, "FisheyeCalibrator");
    qmlRegisterType<ConnectTilesProxyModel>("nx.vms.client.desktop", 1, 0, "ConnectTilesModel");
    qmlRegisterType<PerformanceInfo>("nx.vms.client.desktop", 1, 0, "PerformanceInfo");
    qmlRegisterType<RhiRenderingItem>("nx.vms.client.desktop", 1, 0, "RhiRenderingItem");

    qmlRegisterUncreatableType<Workbench>("nx.vms.client.desktop", 1, 0, "Workbench",
        "Cannot create instance of Workbench.");
    qmlRegisterUncreatableType<QnWorkbenchContext>("nx.vms.client.desktop", 1, 0,
        "WorkbenchContext", "Cannot create instance of WorkbenchContext.");
    qmlRegisterUncreatableType<QnWorkbenchLayout>("nx.vms.client.desktop", 1, 0, "WorkbenchLayout",
        "Cannot create instance of WorkbenchLayout.");

    qmlRegisterUncreatableType<joystick::JoystickButtonSettingsModel>(
        "nx.vms.client.desktop", 1, 0, "JoystickButtonSettingsModel",
        "JoystickButtonSettingsModel can be created from C++ code only.");

    qmlRegisterUncreatableType<joystick::JoystickButtonActionChoiceModel>(
        "nx.vms.client.desktop", 1, 0, "JoystickButtonActionChoiceModel",
        "JoystickButtonActionChoiceModel can be created from C++ code only.");

    qmlRegisterUncreatableType<AccessSubjectEditingContext>("nx.vms.client.desktop", 1, 0,
        "AccessSubjectEditingContext",
        "AccessSubjectEditingContext can be created from C++ code only.");

    qmlRegisterUncreatableType<FilteredResourceProxyModel>("nx.vms.client.desktop", 1, 0,
        "FilteredResourceProxyModel",
        "FilteredResourceProxyModel can be created from C++ code only.");

    qmlRegisterSingletonType<HelpHandler>("nx.vms.client.desktop", 1, 0, "HelpHandler",
        [](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) -> QObject*
        {
            return core::withCppOwnership(&HelpHandler::instance());
        });
    HelpTopic::registerQmlType();

    qmlRegisterSingletonType<LocalSettings>("nx.vms.client.desktop", 1, 0, "LocalSettings",
        [](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) -> QObject*
        {
            return core::withCppOwnership(appContext()->localSettings());
        });

    qmlRegisterSingletonType<WebPageDataCache>("nx.vms.client.desktop", 1, 0, "WebPageDataCache",
        [](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) -> QObject*
        {
            return core::withCppOwnership(appContext()->webPageDataCache());
        });

    ui::scene::Instrument::registerQmlType();
    CursorManager::registerQmlType();
    CustomCursor::registerQmlType();
    CustomCursors::registerQmlType(appContext()->cursors());
    FocusFrameItem::registerQmlType();
    MotionRegionsItem::registerQmlType();
    GlobalToolTip::registerQmlType();
    CursorOverride::registerQmlType();
    ScreenRecordingWatcher::registerQmlType();
    utils::WebEngineProfileManager::registerQmlType();
    workbench::timeline::registerQmlType();
    rules::KeyValueModel::registerQmlType();
    rules::RulesTableModel::registerQmlType();
    rules::RulesSortFilterProxyModel::registerQmlType();
    SortFilterProxyModel::registerQmlType();
    ldap_settings::registerQmlType();
    BlurMaskPreview::registerQmlType();
}

void registerQmlTypes()
{
    static std::once_flag registered;
    std::call_once(registered, &registerQmlTypesInternal);
}

} // namespace nx::vms::client::desktop

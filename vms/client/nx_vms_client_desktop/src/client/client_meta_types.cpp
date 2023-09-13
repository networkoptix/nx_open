// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_meta_types.h"

#include <atomic>

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <common/common_meta_types.h>
#include <common/common_module.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_description.h>
#include <nx/cloud/db/api/result_code.h>
#include <nx/cloud/db/api/system_data.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/models/index_list_model.h>
#include <nx/vms/client/desktop/common/models/linearization_list_model.h>
#include <nx/vms/client/desktop/common/utils/audio_dispatcher.h>
#include <nx/vms/client/desktop/common/utils/list_navigation_helper.h>
#include <nx/vms/client/desktop/common/utils/qml_client_state_selegate.h>
#include <nx/vms/client/desktop/common/utils/quick_message_box.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/debug_utils/components/performance_info.h>
#include <nx/vms/client/desktop/debug_utils/dialogs/joystick_investigation_wizard/joystick_manager.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/joystick/dialog/joystick_button_action_choice_model.h>
#include <nx/vms/client/desktop/joystick/dialog/joystick_button_settings_model.h>
#include <nx/vms/client/desktop/lookup_lists/lookup_lists_dialog.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_status_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/filtering/filtered_resource_proxy_model.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_model_adapter.h>
#include <nx/vms/client/desktop/resource_properties/camera/widgets/motion_regions_item.h>
#include <nx/vms/client/desktop/resource_properties/fisheye/fisheye_calibrator.h>
#include <nx/vms/client/desktop/resource_properties/schedule/record_schedule_cell_data.h>
#include <nx/vms/client/desktop/resource_properties/user/models/resource_access_rights_model.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_rights_list.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/limited_model.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/parent_groups_provider.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/item_view_drag_and_drop_scroll_assist.h>
#include <nx/vms/client/desktop/rules/model_view/rules_sort_filter_proxy_model.h>
#include <nx/vms/client/desktop/rules/model_view/rules_table_model.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_administration/globals/user_settings_global.h>
#include <nx/vms/client/desktop/system_administration/models/custom_access_summary_model.h>
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
#include <nx/vms/client/desktop/thumbnails/resource_id_thumbnail.h>
#include <nx/vms/client/desktop/thumbnails/roi_camera_thumbnail.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
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
#include <nx/vms/client/desktop/ui/common/recording_status_helper.h>
#include <nx/vms/client/desktop/ui/common/whats_this.h>
#include <nx/vms/client/desktop/ui/image_providers/web_page_icon_cache.h>
#include <nx/vms/client/desktop/ui/right_panel/models/right_panel_models_adapter.h>
#include <nx/vms/client/desktop/ui/scene/instruments/instrument.h>
#include <nx/vms/client/desktop/ui/scene/item_model_utils.h>
#include <nx/vms/client/desktop/ui/scene/models/layout_model.h>
#include <nx/vms/client/desktop/ui/scene/models/resource_tree_model_adapter.h>
#include <nx/vms/client/desktop/utils/cursor_manager.h>
#include <nx/vms/client/desktop/utils/date_validator.h>
#include <nx/vms/client/desktop/utils/mouse_spy.h>
#include <nx/vms/client/desktop/utils/server_file_cache.h>
#include <nx/vms/client/desktop/utils/session_aware_attached.h>
#include <nx/vms/client/desktop/utils/upload_state.h>
#include <nx/vms/client/desktop/utils/virtual_camera_payload.h>
#include <nx/vms/client/desktop/utils/virtual_camera_state.h>
#include <nx/vms/client/desktop/utils/webengine_profile_manager.h>
#include <nx/vms/client/desktop/workbench/state/thumbnail_search_state.h>
#include <nx/vms/client/desktop/workbench/timeline/archive_frame_extractor.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail.h>
#include <nx/vms/client/desktop/workbench/timeline/timeline_globals.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_context.h>
#include <recording/time_period.h>
#include <ui/common/notification_levels.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/color_space/image_correction.h>
#include <utils/ping_utility.h>

Q_DECLARE_METATYPE(nx::cloud::db::api::ResultCode)
Q_DECLARE_METATYPE(nx::cloud::db::api::SystemData)
Q_DECLARE_METATYPE(rest::ServerConnectionPtr)

using namespace nx::vms::client::desktop;

namespace {

QnResourcePtr stringToResource(const QString& s)
{
    const QStringList files{s};
    const auto acceptedFiles = QnFileProcessor::findAcceptedFiles(files);
    const auto resourcePool = qnClientCoreModule->resourcePool();
    if (acceptedFiles.empty())
    {
        return resourcePool->getResourceById(QnUuid::fromStringSafe(s));
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
    const auto resourcePool = qnClientCoreModule->resourcePool();
    if (acceptedFiles.empty())
    {
        const auto resource = resourcePool->getResourceById(QnUuid::fromStringSafe(s));
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
        ids << r->getId().toString();
    return ids.join(',');
}

QString uuidToString(const QnUuid& uuid)
{
    return uuid.toString();
}

} // namespace

void QnClientMetaTypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    nx::vms::client::core::initializeMetaTypes();

    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<QVector<QColor> >();
    qRegisterMetaType<QValidator::State>();

    qRegisterMetaType<LayoutResourcePtr>();

    qRegisterMetaType<ResourceTree::NodeType>();
    qRegisterMetaType<Qn::ItemRole>();
    qRegisterMetaType<Qn::ItemDataRole>();
    qRegisterMetaType<workbench::timeline::ThumbnailPtr>();
    qRegisterMetaType<Qn::TimeMode>();
    qRegisterMetaType<Qn::ImageBehavior>();
    qRegisterMetaType<ui::action::IDType>("ui::action::IDType");
    qRegisterMetaType<ui::action::Parameters>();
    qRegisterMetaType<ThumbnailsSearchState>();

    qRegisterMetaType<Qn::LightModeFlags>();
    qRegisterMetaType<Qn::ThumbnailStatus>();

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

    qRegisterMetaType<QnNotificationLevel::Value>();

    qRegisterMetaType<nx::vms::common::update::Information>();
    qRegisterMetaType<UpdateDeliveryInfo>();
    qRegisterMetaType<UpdateContents>();

    qRegisterMetaType<nx::vms::client::desktop::ArchiveFrameExtractor::Result>();

    QMetaType::registerConverter<QVariantMap, QnTimePeriod>(variantMapToTimePeriod);
    QMetaType::registerConverter<QString, QnResourcePtr>(stringToResource);
    QMetaType::registerConverter<QString, QnResourceList>(stringToResourceList);
    QMetaType::registerConverter<QnResourceList, QString>(resourceListToString);
    QMetaType::registerConverter<QnUuid, QString>(uuidToString);
    QMetaType::registerConverter<QnCameraBookmarkList, QVariantList>(bookmarkListToVariantList);
    QMetaType::registerConverter<QVariantList, QnCameraBookmarkList>(variantListToBookmarkList);
    QMetaType::registerConverter<QnCameraBookmark, QString>(bookmarkToString);

    qRegisterMetaType<QnServerFields>();

    WebViewController::registerMetaType();

    QnJsonSerializer::registerSerializer<Qn::ImageBehavior>();

    qRegisterMetaType<RecordScheduleCellData>();
    qRegisterMetaType<nx::vms::client::desktop::RecordScheduleCellData>();

    registerQmlTypes();
}

void QnClientMetaTypes::registerQmlTypes()
{
    SystemContext::registerQmlType();
    AccessRightsList::registerQmlTypes();
    LayoutModel::registerQmlType();
    LinearizationListModel::registerQmlType();
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
    CustomAccessSummaryModel::registerQmlTypes();
    ParentGroupsProvider::registerQmlTypes();
    LimitedModel::registerQmlType();
    GlobalPermissionsModel::registerQmlTypes();
    ResourceIdentificationThumbnail::registerQmlType();
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
    SessionAware::registerQmlType();
    KeyboardModifiers::registerQmlType();
    DateValidator::registerQmlType();

    qmlRegisterType<FisheyeCalibrator>("nx.vms.client.desktop", 1, 0, "FisheyeCalibrator");
    qmlRegisterType<ConnectTilesProxyModel>("nx.vms.client.desktop", 1, 0, "ConnectTilesModel");
    qmlRegisterType<PerformanceInfo>("nx.vms.client.desktop", 1, 0, "PerformanceInfo");

    qmlRegisterUncreatableType<Workbench>("nx.client.desktop", 1, 0, "Workbench",
        "Cannot create instance of Workbench.");
    qmlRegisterUncreatableType<QnWorkbenchContext>("nx.client.desktop", 1, 0, "WorkbenchContext",
        "Cannot create instance of WorkbenchContext.");
    qmlRegisterUncreatableType<QnWorkbenchLayout>("nx.client.desktop", 1, 0, "WorkbenchLayout",
        "Cannot create instance of WorkbenchLayout.");

    qmlRegisterUncreatableType<joystick::JoystickButtonSettingsModel>("nx.vms.client.desktop", 1, 0,
        "JoystickButtonSettingsModel",
        "JoystickButtonSettingsModel can be created from C++ code only.");

    qmlRegisterUncreatableType<joystick::JoystickButtonActionChoiceModel>("nx.vms.client.desktop", 1, 0,
        "JoystickButtonActionChoiceModel",
        "JoystickButtonActionChoiceModel can be created from C++ code only.");

    qmlRegisterUncreatableType<AccessSubjectEditingContext>("nx.vms.client.desktop", 1, 0,
        "AccessSubjectEditingContext",
        "AccessSubjectEditingContext can be created from C++ code only.");

    qmlRegisterUncreatableType<FilteredResourceProxyModel>("nx.vms.client.desktop", 1, 0,
        "FilteredResourceProxyModel",
        "FilteredResourceProxyModel can be created from C++ code only.");

    qmlRegisterSingletonType<HelpHandler>("nx.vms.client.desktop", 1, 0, "HelpHandler",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            auto helpHandler = &HelpHandler::instance();
            qmlEngine->setObjectOwnership(helpHandler, QQmlEngine::CppOwnership);
            return helpHandler;
        });
    HelpTopic::registerQmlType();

    qmlRegisterSingletonType<LocalSettings>("nx.vms.client.desktop", 1, 0, "LocalSettings",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            qmlEngine->setObjectOwnership(appContext()->localSettings(), QQmlEngine::CppOwnership);
            return appContext()->localSettings();
        });

    qmlRegisterSingletonType<WebPageIconCache>("nx.vms.client.desktop", 1, 0, "WebPageIconCache",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            qmlEngine->setObjectOwnership(
                appContext()->webPageIconCache(), QQmlEngine::CppOwnership);

            return appContext()->webPageIconCache();
        });

    ui::scene::Instrument::registerQmlType();
    CursorManager::registerQmlType();
    CustomCursor::registerQmlType();
    CustomCursors::registerQmlType();
    RecordingStatusHelper::registerQmlType();
    FocusFrameItem::registerQmlType();
    MotionRegionsItem::registerQmlType();
    GlobalToolTip::registerQmlType();
    CursorOverride::registerQmlType();
    utils::WebEngineProfileManager::registerQmlType();
    analytics::TaxonomyManager::registerQmlTypes();
    workbench::timeline::registerQmlType();
    rules::RulesTableModel::registerQmlType();
    rules::RulesSortFilterProxyModel::registerQmlType();
}

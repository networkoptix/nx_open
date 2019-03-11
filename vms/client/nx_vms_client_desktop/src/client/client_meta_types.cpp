#include "client_meta_types.h"

#include <atomic>

#include <QtQml/QtQml>

#include <common/common_meta_types.h>

#include <client/client_globals.h>
#include <client/client_model_types.h>
#include <client/client_color_types.h>

#include <camera/thumbnail.h>
#include <camera/data/abstract_camera_data.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <ui/common/notification_levels.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/customization/customization.h>
#include <ui/customization/palette_data.h>
#include <ui/customization/pen_data.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>

#include <utils/color_space/image_correction.h>
#include <nx/fusion/model_functions.h>
#include <utils/ping_utility.h>
#include <nx/vms/client/desktop/utils/server_file_cache.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/vms/client/desktop/resource_views/data/node_type.h>
#include <nx/vms/client/desktop/utils/upload_state.h>
#include <nx/vms/client/desktop/utils/wearable_payload.h>
#include <nx/vms/client/desktop/utils/wearable_state.h>

#include <nx/cloud/db/api/result_code.h>
#include <nx/cloud/db/api/system_data.h>
#include <api/server_rest_connection.h>

#include <nx/vms/client/desktop/resource_properties/camera/widgets/motion_regions_item.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/common/recording_status_helper.h>
#include <nx/vms/client/desktop/ui/common/focus_frame_item.h>
#include <nx/vms/client/desktop/ui/common/global_tool_tip.h>
#include <nx/vms/client/desktop/ui/scene/models/layout_model.h>
#include <nx/vms/client/desktop/ui/scene/instruments/instrument.h>
#include <nx/vms/client/desktop/utils/cursor_manager.h>

QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::TimeMode)

Q_DECLARE_METATYPE(nx::cloud::db::api::ResultCode)
Q_DECLARE_METATYPE(nx::cloud::db::api::SystemData)
Q_DECLARE_METATYPE(rest::QnConnectionPtr)

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, Alignment)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, DateFormat)

using namespace nx::vms::client::desktop;

void QnClientMetaTypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    nx::vms::client::core::initializeMetaTypes();

    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<QVector<QnUuid> >();
    qRegisterMetaType<QVector<QColor> >();
    qRegisterMetaType<QValidator::State>();

    qRegisterMetaTypeStreamOperators<QList<QUrl>>();

    qRegisterMetaType<ResourceTreeNodeType>();
    qRegisterMetaType<Qn::ItemRole>();
    qRegisterMetaType<Qn::ItemDataRole>();
    qRegisterMetaType<QnThumbnail>();
    qRegisterMetaType<QnLicenseWarningState>();
    qRegisterMetaTypeStreamOperators<QnLicenseWarningState>();
    qRegisterMetaType<QnLicenseWarningStateHash>();
    qRegisterMetaTypeStreamOperators<QnLicenseWarningStateHash>();
    qRegisterMetaType<QnServerStorageKey>();
    qRegisterMetaTypeStreamOperators<QnServerStorageKey>();
    qRegisterMetaType<Qn::TimeMode>();
    qRegisterMetaTypeStreamOperators<Qn::TimeMode>();
    qRegisterMetaType<QnBackgroundImage>();
    qRegisterMetaType<Qn::ImageBehaviour>();
    qRegisterMetaType<ui::action::IDType>();
    qRegisterMetaType<ui::action::Parameters>();

    qRegisterMetaType<Qn::LightModeFlags>();
    qRegisterMetaType<Qn::ThumbnailStatus>();

    qRegisterMetaType<QnWeakObjectHash>();
    qRegisterMetaType<WeakGraphicsItemPointerList>();
    qRegisterMetaType<QnCustomization>();
    qRegisterMetaType<QnPingUtility::PingResponce>();
    qRegisterMetaType<nx::vms::client::desktop::ServerFileCache::OperationResult>();

    qRegisterMetaType<QnTimeSliderColors>();
    qRegisterMetaType<QnBackgroundColors>();
    qRegisterMetaType<QnBookmarkColors>();
    qRegisterMetaType<QnCalendarColors>();
    qRegisterMetaType<QnStatisticsColors>();
    qRegisterMetaType<QnScheduleGridColors>();
    qRegisterMetaType<QnGridColors>();
    qRegisterMetaType<QnHistogramColors>();
    qRegisterMetaType<QnTwoWayAudioWidgetColors>();
    qRegisterMetaType<QnResourceWidgetFrameColors>();
    qRegisterMetaType<QnPtzManageModelColors>();
    qRegisterMetaType<QnRoutingManagementColors>();
    qRegisterMetaType<QnAuditLogColors>();
    qRegisterMetaType<QnRecordingStatsColors>();
    qRegisterMetaType<QnVideowallManageWidgetColors>();
    qRegisterMetaType<QnServerUpdatesColors>();
    qRegisterMetaType<QnBackupScheduleColors>();
    qRegisterMetaType<QnFailoverPriorityColors>();
    qRegisterMetaType<QnGraphicsMessageBoxColors>();
    qRegisterMetaType<QnResourceItemColors>();
    qRegisterMetaType<QnPasswordStrengthColors>();

    qRegisterMetaType<QnAbstractCameraDataPtr>();

    qRegisterMetaType<UploadState>();
    qRegisterMetaType<WearableState>();
    qRegisterMetaType<WearableUpload>();

    qRegisterMetaType<nx::cloud::db::api::ResultCode>();
    qRegisterMetaType<nx::cloud::db::api::SystemData>();
    qRegisterMetaType<rest::QnConnectionPtr>();

    qRegisterMetaType<nx::vms::client::desktop::ExportMediaPersistentSettings>();

    qRegisterMetaType<QnNotificationLevel::Value>();

    qRegisterMetaType<nx::update::Information>();
    qRegisterMetaType<nx::update::UpdateContents>();

    QMetaType::registerComparators<QnUuid>();

    QnJsonSerializer::registerSerializer<QnBookmarkColors>();
    QnJsonSerializer::registerSerializer<QnTimeSliderColors>();
    QnJsonSerializer::registerSerializer<QnBackgroundColors>();
    QnJsonSerializer::registerSerializer<QnCalendarColors>();
    QnJsonSerializer::registerSerializer<QnStatisticsColors>();
    QnJsonSerializer::registerSerializer<QnScheduleGridColors>();
    QnJsonSerializer::registerSerializer<QnGridColors>();
    QnJsonSerializer::registerSerializer<QnHistogramColors>();
    QnJsonSerializer::registerSerializer<QnTwoWayAudioWidgetColors>();
    QnJsonSerializer::registerSerializer<QnResourceWidgetFrameColors>();
    QnJsonSerializer::registerSerializer<QnPtzManageModelColors>();
    QnJsonSerializer::registerSerializer<QnRoutingManagementColors>();
    QnJsonSerializer::registerSerializer<QnAuditLogColors>();
    QnJsonSerializer::registerSerializer<QnRecordingStatsColors>();
    QnJsonSerializer::registerSerializer<QnVideowallManageWidgetColors>();
    QnJsonSerializer::registerSerializer<QnServerUpdatesColors>();
    QnJsonSerializer::registerSerializer<QnBackupScheduleColors>();
    QnJsonSerializer::registerSerializer<QnFailoverPriorityColors>();
    QnJsonSerializer::registerSerializer<QnGraphicsMessageBoxColors>();
    QnJsonSerializer::registerSerializer<QnResourceItemColors>();
    QnJsonSerializer::registerSerializer<QnPasswordStrengthColors>();
    QnJsonSerializer::registerSerializer<Qn::ImageBehaviour>();
    QnJsonSerializer::registerSerializer<QnBackgroundImage>();
    QnJsonSerializer::registerSerializer<QnPaletteData>();
    QnJsonSerializer::registerSerializer<QnPenData>();
    QnJsonSerializer::registerSerializer<QVector<QColor> >(); // TODO: #Elric integrate with QVariant iteration?
    QnJsonSerializer::registerSerializer<QVector<QnUuid> >();

    registerQmlTypes();
}

void QnClientMetaTypes::registerQmlTypes()
{
    qmlRegisterType<ColorTheme>("Nx", 1, 0, "ColorThemeBase");
    LayoutModel::registerQmlType();

    qmlRegisterUncreatableType<QnWorkbench>("nx.client.desktop", 1, 0, "Workbench",
        lit("Cannot create instance of Workbench."));
    qmlRegisterUncreatableType<QnWorkbenchContext>("nx.client.desktop", 1, 0, "WorkbenchContext",
        lit("Cannot create instance of WorkbenchContext."));
    qmlRegisterUncreatableType<QnWorkbenchLayout>("nx.client.desktop", 1, 0, "WorkbenchLayout",
        lit("Cannot create instance of WorkbenchLayout."));

    ui::scene::Instrument::registerQmlType();
    CursorManager::registerQmlType();
    RecordingStatusHelper::registerQmlType();
    FocusFrameItem::registerQmlType();
    MotionRegionsItem::registerQmlType();
    GlobalToolTip::registerQmlType();
}


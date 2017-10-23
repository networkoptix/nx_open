#include "client_meta_types.h"

#include <QtQml/QtQml>

#include <common/common_meta_types.h>

#include <client/client_globals.h>
#include <client/client_model_types.h>
#include <client/client_color_types.h>

#include <camera/thumbnail.h>
#include <camera/data/abstract_camera_data.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/customization/customization.h>
#include <ui/customization/palette_data.h>
#include <ui/customization/pen_data.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

#include <update/updates_common.h>
#include <update/update_info.h>

#include <utils/color_space/image_correction.h>
#include <nx/fusion/serialization/json_functions.h>
#include <utils/ping_utility.h>
#include <nx/client/desktop/utils/server_file_cache.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/cloud/cdb/api/system_data.h>
#include <api/server_rest_connection.h>

#include <nx/client/desktop/ui/common/color_theme.h>
#include <nx/client/desktop/ui/common/recording_status_helper.h>
#include <nx/client/desktop/ui/scene/models/layout_model.h>
#include <nx/client/desktop/ui/scene/instruments/instrument.h>
#include <nx/client/desktop/utils/cursor_manager.h>

namespace {

volatile bool qn_clientMetaTypes_initialized = false;

} // namespace

QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::TimeMode)

Q_DECLARE_METATYPE(nx::cdb::api::ResultCode)
Q_DECLARE_METATYPE(nx::cdb::api::SystemData)
Q_DECLARE_METATYPE(rest::QnConnectionPtr)

using namespace nx::client::desktop;

void QnClientMetaTypes::initialize() {
    /* Note that running the code twice is perfectly OK,
     * so we don't need heavyweight synchronization here. */
    if(qn_clientMetaTypes_initialized)
        return;

    QnCommonMetaTypes::initialize();
    nx::client::core::initializeMetaTypes();

    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<QVector<QnUuid> >();
    qRegisterMetaType<QVector<QColor> >();
    qRegisterMetaType<QValidator::State>();

    qRegisterMetaTypeStreamOperators<QList<QUrl>>();

    qRegisterMetaType<Qn::NodeType>();
    qRegisterMetaType<Qn::ItemRole>();
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
    qRegisterMetaType<ImageCorrectionParams>();
    qRegisterMetaType<ui::action::IDType>();
    qRegisterMetaType<ui::action::Parameters>();
    qRegisterMetaType<QnUpdateInfo>();
    qRegisterMetaTypeStreamOperators<QnUpdateInfo>();

    qRegisterMetaType<Qn::LightModeFlags>();
    qRegisterMetaType<Qn::ThumbnailStatus>();

    qRegisterMetaType<QnWeakObjectHash>();
    qRegisterMetaType<WeakGraphicsItemPointerList>();
    qRegisterMetaType<QnCustomization>();
    qRegisterMetaType<QnPingUtility::PingResponce>();
    qRegisterMetaType<nx::client::desktop::ServerFileCache::OperationResult>();

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

    qRegisterMetaType<QnPeerUpdateStage>();
    qRegisterMetaType<QnFullUpdateStage>();
    qRegisterMetaType<QnUpdateResult>();
    qRegisterMetaType<QnCheckForUpdateResult>();

    qRegisterMetaType<nx::cdb::api::ResultCode>();
    qRegisterMetaType<nx::cdb::api::SystemData>();
    qRegisterMetaType<rest::QnConnectionPtr>();

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

    qn_clientMetaTypes_initialized = true;
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
}


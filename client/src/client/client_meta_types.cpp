#include "client_meta_types.h"

#include <common/common_meta_types.h>

#include <client/client_globals.h>
#include <client/client_model_types.h>
#include <client/client_color_types.h>

#include <camera/thumbnail.h>
#include <camera/data/abstract_camera_data.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>
#include <ui/common/weak_graphics_item_pointer.h>
#include <ui/customization/customization.h>
#include <ui/customization/palette_data.h>
#include <ui/customization/pen_data.h>

#include <update/updates_common.h>
#include <update/update_info.h>

#include <utils/color_space/image_correction.h>
#include <nx/fusion/serialization/json_functions.h>
#include <utils/ping_utility.h>
#include <utils/app_server_file_cache.h>

#include <core/core_meta_types.h>

namespace {
    volatile bool qn_clientMetaTypes_initialized = false;

}

QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::TimeMode)
QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::ClientSkin)

void QnClientMetaTypes::initialize() {
    /* Note that running the code twice is perfectly OK,
     * so we don't need heavyweight synchronization here. */
    if(qn_clientMetaTypes_initialized)
        return;

    QnCommonMetaTypes::initialize();
    QnCoreMetaTypes::initialize();

    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<QVector<QnUuid> >();
    qRegisterMetaType<QVector<QColor> >();

    qRegisterMetaTypeStreamOperators<QList<QUrl>>();

    qRegisterMetaType<Qn::NodeType>();
    qRegisterMetaType<Qn::ItemRole>();
    qRegisterMetaType<QnThumbnail>();
    qRegisterMetaType<QnWorkbenchState>();
    qRegisterMetaTypeStreamOperators<QnWorkbenchState>();
    qRegisterMetaType<QnWorkbenchStateHash>();
    qRegisterMetaTypeStreamOperators<QnWorkbenchStateHash>();
    qRegisterMetaType<QnLicenseWarningState>();
    qRegisterMetaTypeStreamOperators<QnLicenseWarningState>();
    qRegisterMetaType<QnLicenseWarningStateHash>();
    qRegisterMetaTypeStreamOperators<QnLicenseWarningStateHash>();
    qRegisterMetaType<QnServerStorageKey>();
    qRegisterMetaTypeStreamOperators<QnServerStorageKey>();
    qRegisterMetaType<Qn::TimeMode>();
    qRegisterMetaTypeStreamOperators<Qn::TimeMode>();
    qRegisterMetaType<Qn::ClientSkin>();
    qRegisterMetaTypeStreamOperators<Qn::ClientSkin>();
    qRegisterMetaType<Qn::BackgroundAnimationMode>();
    qRegisterMetaTypeStreamOperators<Qn::BackgroundAnimationMode>();
    qRegisterMetaType<QnClientBackground>();
    qRegisterMetaTypeStreamOperators<QnClientBackground>();
    qRegisterMetaType<Qn::ImageBehaviour>();
    qRegisterMetaTypeStreamOperators<Qn::ImageBehaviour>();
    qRegisterMetaType<ImageCorrectionParams>();
    qRegisterMetaType<QnActions::IDType>();
    qRegisterMetaType<QnActionParameters>();
    qRegisterMetaType<QnAspectRatioHash>();
    qRegisterMetaTypeStreamOperators<QnAspectRatioHash>();
    qRegisterMetaType<QnUpdateInfo>();
    qRegisterMetaTypeStreamOperators<QnUpdateInfo>();

    qRegisterMetaType<Qn::LightModeFlags>();

    qRegisterMetaType<QnWeakObjectHash>();
    qRegisterMetaType<WeakGraphicsItemPointerList>();
    qRegisterMetaType<QnCustomization>();
    qRegisterMetaType<QnPingUtility::PingResponce>();
    qRegisterMetaType<QnAppServerFileCache::OperationResult>();

    qRegisterMetaType<QnTimeSliderColors>();
    qRegisterMetaType<QnTimeScrollBarColors>();
    qRegisterMetaType<QnBackgroundColors>();
    qRegisterMetaType<QnBookmarkColors>();
    qRegisterMetaType<QnCalendarColors>();
    qRegisterMetaType<QnStatisticsColors>();
    qRegisterMetaType<QnIoModuleColors>();
    qRegisterMetaType<QnScheduleGridColors>();
    qRegisterMetaType<QnGridColors>();
    qRegisterMetaType<QnHistogramColors>();
    qRegisterMetaType<QnTwoWayAudioWidgetColors>();
    qRegisterMetaType<QnResourceWidgetFrameColors>();
    qRegisterMetaType<QnCompositeTextOverlayColors>();
    qRegisterMetaType<QnPtzManageModelColors>();
    qRegisterMetaType<QnLicensesListModelColors>();
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

    QnJsonSerializer::registerSerializer<QnBookmarkColors>();
    QnJsonSerializer::registerSerializer<QnTimeSliderColors>();
    QnJsonSerializer::registerSerializer<QnTimeScrollBarColors>();
    QnJsonSerializer::registerSerializer<QnBackgroundColors>();
    QnJsonSerializer::registerSerializer<QnCalendarColors>();
    QnJsonSerializer::registerSerializer<QnStatisticsColors>();
    QnJsonSerializer::registerSerializer<QnIoModuleColors>();
    QnJsonSerializer::registerSerializer<QnScheduleGridColors>();
    QnJsonSerializer::registerSerializer<QnGridColors>();
    QnJsonSerializer::registerSerializer<QnHistogramColors>();
    QnJsonSerializer::registerSerializer<QnTwoWayAudioWidgetColors>();
    QnJsonSerializer::registerSerializer<QnResourceWidgetFrameColors>();
    QnJsonSerializer::registerSerializer<QnCompositeTextOverlayColors>();
    QnJsonSerializer::registerSerializer<QnPtzManageModelColors>();
    QnJsonSerializer::registerSerializer<QnLicensesListModelColors>();
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

    QnJsonSerializer::registerSerializer<Qn::ClientSkin>();
    QnJsonSerializer::registerSerializer<Qn::BackgroundAnimationMode>();
    QnJsonSerializer::registerSerializer<Qn::ImageBehaviour>();
    QnJsonSerializer::registerSerializer<QnPaletteData>();
    QnJsonSerializer::registerSerializer<QnPenData>();
    QnJsonSerializer::registerSerializer<QVector<QColor> >(); // TODO: #Elric integrate with QVariant iteration?
    QnJsonSerializer::registerSerializer<QVector<QnUuid> >();

    qn_clientMetaTypes_initialized = true;
}


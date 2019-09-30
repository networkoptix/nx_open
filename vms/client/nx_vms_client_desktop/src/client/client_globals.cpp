#include "client_globals.h"
#include <nx/fusion/serialization/lexical_enum.h>

/**
 * Those roles are used in the BlueBox integration and shall not be changed without an aggreement.
 */
static_assert(Qn::ItemGeometryRole == 288);
static_assert(Qn::ItemImageDewarpingRole == 295);
static_assert(Qn::ItemRotationRole == 297);
static_assert(Qn::ItemFlipRole == 299);

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, ThumbnailStatus,
(Qn::ThumbnailStatus::Invalid, "Invalid")
(Qn::ThumbnailStatus::Loading, "Loading")
(Qn::ThumbnailStatus::Loaded, "Loaded")
(Qn::ThumbnailStatus::NoData, "NoData")
(Qn::ThumbnailStatus::Refreshing, "Refreshing")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, ItemDataRole,
(Qn::ItemDataRole::FirstItemDataRole, "FirstItemDataRole")

/* Tree-based. */
(Qn::ItemDataRole::NodeTypeRole, "NodeTypeRole")
(Qn::ItemDataRole::ResourceTreeScopeRole, "ResourceTreeScopeRole")

/* Resource-based. */
(Qn::ItemDataRole::ResourceRole, "ResourceRole")
(Qn::ItemDataRole::UserResourceRole, "UserResourceRole")
(Qn::ItemDataRole::LayoutResourceRole, "LayoutResourceRole")
(Qn::ItemDataRole::MediaServerResourceRole, "MediaServerResourceRole")
(Qn::ItemDataRole::VideoWallResourceRole, "VideoWallResourceRole")

(Qn::ItemDataRole::ResourceNameRole, "ResourceNameRole")
(Qn::ItemDataRole::ResourceFlagsRole, "ResourceFlagsRole")
(Qn::ItemDataRole::ResourceStatusRole, "ResourceStatusRole")
(Qn::ItemDataRole::ResourceIconKeyRole, "ResourceIconKeyRole")

(Qn::ItemDataRole::CameraExtraStatusRole, "CameraExtraStatusRole")

(Qn::ItemDataRole::VideoWallGuidRole, "VideoWallGuidRole")
(Qn::ItemDataRole::VideoWallItemGuidRole, "VideoWallItemGuidRole")
(Qn::ItemDataRole::VideoWallItemIndicesRole, "VideoWallItemIndicesRole")

(Qn::ItemDataRole::LayoutTourUuidRole, "LayoutTourUuidRole")
(Qn::ItemDataRole::LayoutTourItemDelayMsRole, "LayoutTourItemDelayMsRole")
(Qn::ItemDataRole::LayoutTourItemOrderRole, "LayoutTourItemOrderRole")
(Qn::ItemDataRole::LayoutTourIsManualRole, "LayoutTourIsManualRole")

/* Layout-based. */
(Qn::ItemDataRole::LayoutCellSpacingRole, "LayoutCellSpacingRole")
(Qn::ItemDataRole::LayoutCellAspectRatioRole, "LayoutCellAspectRatioRole")
(Qn::ItemDataRole::LayoutBoundingRectRole, "LayoutBoundingRectRole")
(Qn::ItemDataRole::LayoutMinimalBoundingRectRole, "LayoutMinimalBoundingRectRole")
(Qn::ItemDataRole::LayoutSyncStateRole, "LayoutSyncStateRole")
(Qn::ItemDataRole::LayoutSearchStateRole, "LayoutSearchStateRole")
(Qn::ItemDataRole::LayoutTimeLabelsRole, "LayoutTimeLabelsRole")
(Qn::ItemDataRole::LayoutPermissionsRole, "LayoutPermissionsRole")
(Qn::ItemDataRole::LayoutSelectionRole, "LayoutSelectionRole")
(Qn::ItemDataRole::LayoutActiveItemRole, "LayoutActiveItemRole")
(Qn::ItemDataRole::LayoutWatermarkRole, "LayoutWatermarkRole")

/* Item-based. */
(Qn::ItemDataRole::ItemUuidRole, "ItemUuidRole")
(Qn::ItemDataRole::ItemGeometryRole, "ItemGeometryRole")
(Qn::ItemDataRole::ItemGeometryDeltaRole, "ItemGeometryDeltaRole")
(Qn::ItemDataRole::ItemCombinedGeometryRole, "ItemCombinedGeometryRole")
(Qn::ItemDataRole::ItemPositionRole, "ItemPositionRole")
(Qn::ItemDataRole::ItemZoomRectRole, "ItemZoomRectRole")
(Qn::ItemDataRole::ItemZoomWindowRectangleVisibleRole, "ItemZoomWindowRectangleVisibleRole")
(Qn::ItemDataRole::ItemImageEnhancementRole, "ItemImageEnhancementRole")
(Qn::ItemDataRole::ItemImageDewarpingRole, "ItemImageDewarpingRole")
(Qn::ItemDataRole::ItemFlagsRole, "ItemFlagsRole")
(Qn::ItemDataRole::ItemRotationRole, "ItemRotationRole")
(Qn::ItemDataRole::ItemFrameDistinctionColorRole, "ItemFrameDistinctionColorRole")
(Qn::ItemDataRole::ItemFlipRole, "ItemFlipRole")
(Qn::ItemDataRole::ItemAspectRatioRole, "ItemAspectRatioRole")
(Qn::ItemDataRole::ItemDisplayInfoRole, "ItemDisplayInfoRole")
(Qn::ItemDataRole::ItemPlaceholderRole, "ItemPlaceholderRole")

(Qn::ItemDataRole::ItemTimeRole, "ItemTimeRole")
(Qn::ItemDataRole::ItemPausedRole, "ItemPausedRole")
(Qn::ItemDataRole::ItemSpeedRole, "ItemSpeedRole")
(Qn::ItemDataRole::ItemSliderWindowRole, "ItemSliderWindowRole")
(Qn::ItemDataRole::ItemSliderSelectionRole, "ItemSliderSelectionRole")
(Qn::ItemDataRole::ItemCheckedButtonsRole, "ItemCheckedButtonsRole")
(Qn::ItemDataRole::ItemDisabledButtonsRole, "ItemDisabledButtonsRole")
(Qn::ItemDataRole::ItemHealthMonitoringButtonsRole, "ItemHealthMonitoringButtonsRole")
(Qn::ItemDataRole::ItemMotionSelectionRole, "ItemMotionSelectionRole")
(Qn::ItemDataRole::ItemAnalyticsSelectionRole, "ItemAnalyticsSelectionRole")

/* Ptz-based. */
(Qn::ItemDataRole::PtzPresetRole, "PtzPresetRole")
(Qn::ItemDataRole::PtzTourRole, "PtzTourRole")
(Qn::ItemDataRole::PtzObjectIdRole, "PtzObjectIdRole")
(Qn::ItemDataRole::PtzObjectNameRole, "PtzObjectNameRole")
(Qn::ItemDataRole::PtzTourSpotRole, "PtzTourSpotRole")
(Qn::ItemDataRole::PtzSpeedRole, "PtzSpeedRole")
(Qn::ItemDataRole::PtzPresetIndexRole, "PtzPresetIndexRole")

/* Context-based. */
(Qn::ItemDataRole::CurrentLayoutResourceRole, "CurrentLayoutResourceRole")
(Qn::ItemDataRole::CurrentLayoutMediaItemsRole, "CurrentLayoutMediaItemsRole")

/**
 * Special layout roles
 */

(Qn::ItemDataRole::IsSpecialLayoutRole, "IsSpecialLayoutRole")
(Qn::ItemDataRole::LayoutIconRole, "LayoutIconRole")
(Qn::ItemDataRole::LayoutFlagsRole, "LayoutFlagsRole")
(Qn::ItemDataRole::LayoutMarginsRole, "LayoutMarginsRole")
(Qn::ItemDataRole::CustomPanelTitleRole, "CustomPanelTitleRole")
(Qn::ItemDataRole::CustomPanelDescriptionRole, "CustomPanelDescriptionRole")
(Qn::ItemDataRole::CustomPanelActionsRole, "CustomPanelActionsRole")

(Qn::ItemDataRole::ActionIdRole, "ActionIdRole")


(Qn::ItemDataRole::StartupParametersRole, "StartupParametersRole")

(Qn::ItemDataRole::ConnectionInfoRole, "ConnectionInfoRole")
(Qn::ItemDataRole::FocusElementRole, "FocusElementRole")
(Qn::ItemDataRole::FocusTabRole, "FocusTabRole")
(Qn::ItemDataRole::TimePeriodRole, "TimePeriodRole")
(Qn::ItemDataRole::TimePeriodsRole, "TimePeriodsRole")
(Qn::ItemDataRole::MergedTimePeriodsRole, "MergedTimePeriodsRole")
(Qn::ItemDataRole::FileNameRole, "FileNameRole")
(Qn::ItemDataRole::TextRole, "TextRole")
(Qn::ItemDataRole::IntRole, "IntRole")


(Qn::ItemDataRole::UrlRole, "UrlRole")


(Qn::ItemDataRole::LogonParametersRole, "LogonParametersRole")

/**
 * Role for cloud system id (QString). Used in cloud system nodes in the resources tree and
 * as the ConnectToCloudAction parameter.
 */
(Qn::ItemDataRole::CloudSystemIdRole, "CloudSystemIdRole")


(Qn::ItemDataRole::ForceRole, "ForceRole")

(Qn::ItemDataRole::CameraBookmarkRole, "CameraBookmarkRole")
(Qn::ItemDataRole::CameraBookmarkListRole, "CameraBookmarkListRole")
(Qn::ItemDataRole::BookmarkTagRole, "BookmarkTagRole")
(Qn::ItemDataRole::UuidRole, "UuidRole")
(Qn::ItemDataRole::KeyboardModifiersRole, "KeyboardModifiersRole")

/* Others. */
(Qn::ItemDataRole::HelpTopicIdRole, "HelpTopicIdRole")

(Qn::ItemDataRole::TranslationRole, "TranslationRole")

(Qn::ItemDataRole::ItemMouseCursorRole, "ItemMouseCursorRole")
(Qn::ItemDataRole::DisplayHtmlRole, "DisplayHtmlRole")
(Qn::ItemDataRole::DisplayHtmlHoveredRole, "DisplayHtmlHoveredRole")

(Qn::ItemDataRole::ModifiedRole, "ModifiedRole")
(Qn::ItemDataRole::DisabledRole, "DisabledRole")
(Qn::ItemDataRole::ValidRole, "ValidRole")
(Qn::ItemDataRole::ActionIsInstantRole, "ActionIsInstantRole")
(Qn::ItemDataRole::ShortTextRole, "ShortTextRole")
(Qn::ItemDataRole::PriorityRole, "PriorityRole")

(Qn::ItemDataRole::EventTypeRole, "EventTypeRole")
(Qn::ItemDataRole::EventResourcesRole, "EventResourcesRole")
(Qn::ItemDataRole::ActionTypeRole, "ActionTypeRole")
(Qn::ItemDataRole::ActionResourcesRole, "ActionResourcesRole")
(Qn::ItemDataRole::ActionDataRole, "ActionDataRole")
(Qn::ItemDataRole::RuleModelRole, "RuleModelRole") /* #deprecate #3.2 */
(Qn::ItemDataRole::EventParametersRole, "EventParametersRole")

(Qn::ItemDataRole::StorageUrlRole, "StorageUrlRole")

(Qn::ItemDataRole::IOPortDataRole, "IOPortDataRole")

(Qn::ItemDataRole::AuditRecordDataRole, "AuditRecordDataRole")
(Qn::ItemDataRole::ColumnDataRole, "ColumnDataRole")
(Qn::ItemDataRole::DecorationHoveredRole, "DecorationHoveredRole")
(Qn::ItemDataRole::AlternateColorRole, "AlternateColorRole")
(Qn::ItemDataRole::AuditLogChartDataRole, "AuditLogChartDataRole")

(Qn::ItemDataRole::StorageInfoDataRole, "StorageInfoDataRole")
(Qn::ItemDataRole::BackupSettingsDataRole, "BackupSettingsDataRole")
(Qn::ItemDataRole::TextWidthDataRole, "TextWidthDataRole")

(Qn::ItemDataRole::ActionEmitterType, "ActionEmitterType")
(Qn::ItemDataRole::ActionEmittedBy, "ActionEmittedBy")

(Qn::ItemDataRole::GlobalPermissionsRole, "GlobalPermissionsRole")
(Qn::ItemDataRole::UserRoleRole, "UserRoleRole")

(Qn::ItemDataRole::ValidationStateRole, "ValidationStateRole")
(Qn::ItemDataRole::ResolutionModeRole, "ResolutionModeRole")
(Qn::ItemDataRole::AnalyticsObjectsVisualizationModeRole, "AnalyticsObjectsVisualizationModeRole")

(Qn::ItemDataRole::ForceShowCamerasList, "ForceShowCamerasList")
(Qn::ItemDataRole::ParentWidgetRole, "ParentWidgetRole")

(Qn::ItemDataRole::TimestampRole, "TimestampRole")
(Qn::ItemDataRole::TimestampTextRole, "TimestampTextRole")
(Qn::ItemDataRole::DescriptionTextRole, "DescriptionTextRole")
(Qn::ItemDataRole::AdditionalTextRole, "AdditionalTextRole")
(Qn::ItemDataRole::RemovableRole, "RemovableRole")
(Qn::ItemDataRole::CommandActionRole, "CommandActionRole")
(Qn::ItemDataRole::ResourceListRole, "ResourceListRole")
(Qn::ItemDataRole::DisplayedResourceListRole, "DisplayedResourceListRole")
(Qn::ItemDataRole::PreviewTimeRole, "PreviewTimeRole")
(Qn::ItemDataRole::TimeoutRole, "TimeoutRole")
(Qn::ItemDataRole::BusyIndicatorVisibleRole, "BusyIndicatorVisibleRole")
(Qn::ItemDataRole::ProgressValueRole, "ProgressValueRole")
(Qn::ItemDataRole::DurationRole, "DurationRole")
(Qn::ItemDataRole::NotificationLevelRole, "NotificationLevelRole")
(Qn::ItemDataRole::ContextMenuRole, "ContextMenuRole")
(Qn::ItemDataRole::ForcePrecisePreviewRole, "ForcePrecisePreviewRole")
(Qn::ItemDataRole::PreviewStreamSelectionRole, "PreviewStreamSelectionRole")
(Qn::ItemDataRole::DecorationPathRole, "DecorationPathRole")

(Qn::ItemDataRole::SelectOnOpeningRole, "SelectOnOpeningRole")
(Qn::ItemDataRole::RaiseSelectionRole, "RaiseSelectionRole")

(Qn::ItemDataRole::ExtraResourceInfoRole, "ExtraResourceInfoRole")

// Model notification roles. Do not necessarily pass any data but implement
// item-related view-to-model notifications via setData which can be proxied.
(Qn::ItemDataRole::DefaultNotificationRole, "DefaultNotificationRole")
(Qn::ItemDataRole::ActivateLinkRole, "ActivateLinkRole")

(Qn::ItemDataRole::ItemDataRoleCount, "ItemDataRoleCount")
)

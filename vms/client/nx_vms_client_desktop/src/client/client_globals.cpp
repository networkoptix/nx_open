// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_globals.h"

#include <nx/reflect/enum_string_conversion.h>

namespace Qn {

/**
 * These roles are used in the BlueBox integration and should not be changed without an agreement.
 */
static_assert(ItemDataRole::FirstItemDataRole < ItemDataRole::ItemGeometryRole);
static_assert(ItemGeometryRole == 288);
static_assert(ItemImageDewarpingRole == 295);
static_assert(ItemRotationRole == 297);
static_assert(ItemFlipRole == 299);

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ItemDataRole*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ItemDataRole>;
    return visitor(
        #define IDR_ITEM(I) Item{ItemDataRole::I, #I}

        IDR_ITEM(FirstItemDataRole),

        // Tree-based.
        IDR_ITEM(NodeTypeRole),
        IDR_ITEM(ResourceTreeScopeRole),

        // Resource-based.
        IDR_ITEM(UserResourceRole),
        IDR_ITEM(VideoWallResourceRole),

        IDR_ITEM(ResourceFlagsRole),
        IDR_ITEM(ResourceIconKeyRole),

        IDR_ITEM(ResourceExtraStatusRole),

        IDR_ITEM(VideoWallGuidRole),
        IDR_ITEM(VideoWallItemGuidRole),
        IDR_ITEM(VideoWallItemIndicesRole),

        IDR_ITEM(ShowreelUuidRole),
        IDR_ITEM(ShowreelItemDelayMsRole),
        IDR_ITEM(ShowreelItemOrderRole),
        IDR_ITEM(ShowreelIsManualRole),

        // Layout-based.
        IDR_ITEM(LayoutCellSpacingRole),
        IDR_ITEM(LayoutCellAspectRatioRole),
        IDR_ITEM(LayoutMinimalBoundingRectRole),
        IDR_ITEM(LayoutSyncStateRole),
        IDR_ITEM(LayoutSearchStateRole),
        IDR_ITEM(LayoutTimeLabelsRole),
        IDR_ITEM(LayoutPermissionsRole),
        IDR_ITEM(LayoutSelectionRole),
        IDR_ITEM(LayoutActiveItemRole),
        IDR_ITEM(LayoutWatermarkRole),

        // Item-based.
        IDR_ITEM(ItemUuidRole),
        IDR_ITEM(ItemGeometryRole),
        IDR_ITEM(ItemGeometryDeltaRole),
        IDR_ITEM(ItemCombinedGeometryRole),
        IDR_ITEM(ItemPositionRole),
        IDR_ITEM(ItemZoomRectRole),
        IDR_ITEM(ItemZoomWindowRectangleVisibleRole),
        IDR_ITEM(ItemImageEnhancementRole),
        IDR_ITEM(ItemImageDewarpingRole),
        IDR_ITEM(ItemFlagsRole),
        IDR_ITEM(ItemRotationRole),
        IDR_ITEM(ItemFrameDistinctionColorRole),
        IDR_ITEM(ItemFlipRole),
        IDR_ITEM(ItemAspectRatioRole),
        IDR_ITEM(ItemDisplayInfoRole),
        IDR_ITEM(ItemPtzControlRole),
        IDR_ITEM(ItemDisplayAnalyticsObjectsRole),
        IDR_ITEM(ItemDisplayRoiRole),
        IDR_ITEM(ItemPlaceholderRole),
        IDR_ITEM(ItemDisplayHotspotsRole),

        IDR_ITEM(ItemTimeRole),
        IDR_ITEM(ItemPausedRole),
        IDR_ITEM(ItemSpeedRole),
        IDR_ITEM(ItemSliderWindowRole),
        IDR_ITEM(ItemSliderSelectionRole),
        IDR_ITEM(ItemCheckedButtonsRole),
        IDR_ITEM(ItemDisabledButtonsRole),
        IDR_ITEM(ItemHealthMonitoringButtonsRole),
        IDR_ITEM(ItemMotionSelectionRole),
        IDR_ITEM(ItemAnalyticsSelectionRole),
        IDR_ITEM(ItemWebPageSavedStateDataRole),
        IDR_ITEM(ItemSkipFocusOnAdditionRole),

        /* Ptz-based. */
        IDR_ITEM(PtzPresetRole),
        IDR_ITEM(PtzTourRole),
        IDR_ITEM(PtzObjectIdRole),
        IDR_ITEM(PtzObjectNameRole),
        IDR_ITEM(PtzTourSpotRole),
        IDR_ITEM(PtzSpeedRole),
        IDR_ITEM(PtzPresetIndexRole),

        /* Context-based. */
        IDR_ITEM(CurrentLayoutResourceRole),
        IDR_ITEM(CurrentLayoutMediaItemsRole),

        /**
          * Special layout roles
          */

        IDR_ITEM(IsSpecialLayoutRole),
        IDR_ITEM(LayoutIconRole),
        IDR_ITEM(LayoutFlagsRole),
        IDR_ITEM(LayoutMarginsRole),
        IDR_ITEM(CustomPanelTitleRole),
        IDR_ITEM(CustomPanelDescriptionRole),
        IDR_ITEM(CustomPanelActionsRole),

        IDR_ITEM(ActionIdRole),

        /** Role for command-line startup parameters. */
        IDR_ITEM(StartupParametersRole),

        IDR_ITEM(ConnectionInfoRole),
        IDR_ITEM(FocusElementRole),
        IDR_ITEM(FocusTabRole),
        IDR_ITEM(TimePeriodRole),
        IDR_ITEM(TimePeriodsRole),
        IDR_ITEM(MergedTimePeriodsRole),
        IDR_ITEM(FileNameRole),
        IDR_ITEM(TextRole),
        IDR_ITEM(IntRole),

        /** Role for target url. Used in BrowseUrlAction. */
        IDR_ITEM(UrlRole),

        /** LogonData structure. Used in the ConnectAction. */
        IDR_ITEM(LogonDataRole),

        /**
         * Role for cloud system id (QString). Used in cloud system nodes in the resources tree and
         * as the ConnectToCloudAction parameter.
         */
        IDR_ITEM(CloudSystemIdRole),

        /** Role for 'forced' flag. Used in DisconnectAction. */
        IDR_ITEM(ForceRole),

        IDR_ITEM(CameraBookmarkListRole),
        IDR_ITEM(KeyboardModifiersRole),

        /* Others. */
        IDR_ITEM(HelpTopicIdRole),

        IDR_ITEM(ItemMouseCursorRole),
        IDR_ITEM(DisplayHtmlRole),
        IDR_ITEM(DisplayHtmlHoveredRole),

        IDR_ITEM(ModifiedRole),
        IDR_ITEM(DisabledRole),
        IDR_ITEM(ValidRole),
        IDR_ITEM(ActionIsInstantRole),
        IDR_ITEM(ShortTextRole),
        IDR_ITEM(PriorityRole),

        IDR_ITEM(EventTypeRole),
        IDR_ITEM(EventResourcesRole),
        IDR_ITEM(ActionTypeRole),
        IDR_ITEM(ActionResourcesRole),
        IDR_ITEM(ActionDataRole),
        IDR_ITEM(RuleModelRole),
        IDR_ITEM(EventParametersRole),

        IDR_ITEM(StorageUrlRole),

        IDR_ITEM(IOPortDataRole),

        IDR_ITEM(AuditRecordDataRole),
        IDR_ITEM(ColumnDataRole),
        IDR_ITEM(DecorationHoveredRole),
        IDR_ITEM(AlternateColorRole),
        IDR_ITEM(AuditLogChartDataRole),

        IDR_ITEM(StorageInfoDataRole),
        IDR_ITEM(BackupSettingsDataRole),
        IDR_ITEM(TextWidthDataRole),

        IDR_ITEM(ActionEmitterType),
        IDR_ITEM(ActionEmittedBy),

        IDR_ITEM(GlobalPermissionsRole),

        IDR_ITEM(ValidationStateRole),
        IDR_ITEM(ResolutionModeRole),

        IDR_ITEM(ForceShowCamerasList),
        IDR_ITEM(ParentWidgetRole),

        IDR_ITEM(AdditionalTextRole),
        IDR_ITEM(RemovableRole),
        IDR_ITEM(CommandActionRole),
        IDR_ITEM(TimeoutRole),
        IDR_ITEM(BusyIndicatorVisibleRole),
        IDR_ITEM(ProgressValueRole),
        IDR_ITEM(ProgressFormatRole),
        IDR_ITEM(NotificationLevelRole),
        IDR_ITEM(CreateContextMenuRole),
        IDR_ITEM(ForcePrecisePreviewRole),

        IDR_ITEM(SelectOnOpeningRole),

        IDR_ITEM(ExtraInfoRole),

        IDR_ITEM(AutoExpandRole),

        IDR_ITEM(RawResourceRole),

        /** Enable advanced UI features (typically in settings dialogs). */
        IDR_ITEM(AdvancedModeRole),

        // Additional roles for the resource tree. Placed at the end of the list for purpose
        // not to change enumeration values for BlueBox integration.
        IDR_ITEM(ParentResourceRole),
        IDR_ITEM(CameraGroupIdRole),
        IDR_ITEM(ResourceTreeCustomGroupIdRole),

        IDR_ITEM(ParentNodeTypeRole),
        IDR_ITEM(TopLevelParentNodeTypeRole),
        IDR_ITEM(TargetResourceTreeCustomGroupIdRole),
        IDR_ITEM(OnlyResourceTreeSiblingsRole),

        IDR_ITEM(RemoteConnectionRole),

        IDR_ITEM(ItemDataRoleCount)

        #undef IDR_ITEM
    );
}

std::string toString(ItemDataRole value)
{
    return nx::reflect::enumeration::toString(value);
}

bool fromString(const std::string_view& str, ItemDataRole* value)
{
    return nx::reflect::enumeration::fromString(str, value);
}

} // namespace Qn

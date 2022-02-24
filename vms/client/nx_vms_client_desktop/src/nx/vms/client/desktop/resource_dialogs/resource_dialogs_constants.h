// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <client/client_globals.h>

namespace nx::vms::client::desktop {

enum ResourceDialogItemRole
{
    IsValidResourceRole = Qn::ItemDataRoleCount + 1, //< Type is bool.
    IsItemHighlightedRole, //< Type is bool.
    FailoverPriorityRole, //< Type nx::vms::api::FailoverPriority.
    BackupEnabledRole, //< Type is bool.
    BackupContentTypesRole, //< Type is nx::vms::api::BackupContentTypes.
    BackupQualityRole, //< Type is nx::vms::api::CameraBackupQuality.
    HasDropdownMenuRole, //< Type is bool.
    NothingToBackupWarningRole, //< Type is QString, contains warning explanation.
    NewAddedCamerasItemRole, //< Type is bool.
};

enum class ResourceSelectionMode
{
    MultiSelection, //< Arbitrary selection, may be null. Groups are checkable.
    SingleSelection, //< Single resource or none.
    ExclusiveSelection, //< Exactly one resource, radio group style appearance.
};

namespace resource_selection_view {

enum Column
{
    ResourceColumn,
    CheckboxColumn,

    ColumnCount,
};

} // namespace resource_selection_view

namespace failover_priority_view {

enum Column
{
    ResourceColumn,
    FailoverPriorityColumn,

    ColumnCount,
};

} // namespace failover_priority_view

namespace backup_settings_view {

enum Column
{
    ResourceColumn,
    WarningIconColumn,
    ContentTypesColumn,
    QualityColumn,
    SwitchColumn,

    ColumnCount,
};

} // namespace backup_settings_widget

namespace accessible_media_selection_view {

enum Column
{
    ResourceColumn,
    IndirectResourceAccessIconColumn,
    CheckboxColumn,

    ColumnCount
};

} // namespace accessible_media_selection_view

} // namespace nx::vms::client::desktop

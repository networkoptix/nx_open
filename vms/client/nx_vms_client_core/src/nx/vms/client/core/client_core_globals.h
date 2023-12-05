// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/qnamespace.h>

#include <string_view>

namespace nx::vms::client::core {

enum CoreItemDataRole
{
    FirstCoreItemDataRole = Qt::UserRole,

    TimestampRole,                  /**< Role for timestamp in microseconds since epoch (std::chrono::microseconds). */
    ResourceNameRole,               /**< Role for resource name. Value of type QString. */
    ResourceRole,                   /**< Role for QnResourcePtr. */
    RawResourceRole,                /**< Role for QnResource*. */
    ResourceListRole,               /**< Resource list (QnResourceList). */
    LayoutResourceRole,             /**< Role for QnLayoutResourcePtr. */
    MediaServerResourceRole,        /**< Role for QnMediaServerResourcePtr. */
    ResourceStatusRole,             /**< Role for resource status. Value of type int (nx::vms::api::ResourceStatus). */

    DescriptionTextRole,            /**< Role for generic description text (QString). */
    TimestampTextRole,              /**< Role for timestamp text (QString). */
    DisplayedResourceListRole,      /**< Resource list displayed in a Right Panel tile (QnResourceList or QStringList). */
    PreviewTimeRole,                /**< Role for camera preview time in microseconds since epoch (std::chrono::microseconds). */
    TimestampMsRole,                /**< Role for some timestamp, in milliseconds since epoch (std::chrono::milliseconds). */
    UuidRole,                       /**< Role for target uuid. Used in LoadVideowallMatrixAction. */
    DurationRole,                   /**< Role for duration in microseconds (std::chrono::microseconds). */
    DurationMsRole,                   /**< Role for duration in microseconds (std::chrono::microseconds). */

    CameraBookmarkRole,             /**< Role for the selected camera bookmark (if any). Used in Edit/RemoveBookmarkAction */
    BookmarkTagRole,                /**< Role for bookmark tag. Used in OpenBookmarksSearchAction */

    AnalyticsEngineNameRole,        /**< Role for related analytics engine name. (QString) */

    HasExternalBestShotRole,        /**< Whether object detection track has an external best shot image (bool). */

    /**
     * Model notification roles. Do not necessarily pass any data but implement item-related
     * view-to-model notifications via setData which can be proxied.
     */
    DefaultNotificationRole,        /**< Role to perform default item action (no data). */
    ActivateLinkRole,               /**< Role to parse and follow hyperlink (QString). */
    DecorationPathRole,             /**< Role for icon path (QString). */
    AnalyticsAttributesRole,        /**< Role for analytics attribute lists (QList<nx::vms::client::desktop::analytics::Attribute>). */
    FlatAttributeListRole,          /**< Flattened to QStringList grouped attribute lists (nx::common::metadata::GroupedAttributes). */
    RawAttributeListRole,
    ObjectTrackIdRole,              /**< Role for camera preview stream (CameraImageRequest::objectTrackId). */
    PreviewStreamSelectionRole,     /**< Role for camera preview stream (ImageRequest::StreamSelectionMode). */

    IpAddressRole,                  /**< Role for ip address. Value of type QUrl. */
    ThumbnailRole,                  /**< Role for thumbnail. Value of type QUrl. */

    CoreItemDataRoleCount
};

//** Returns role id to text name mapping for the some roles. */
NX_VMS_CLIENT_CORE_API QHash<int, QByteArray> clientCoreRoleNames();

std::string toString(CoreItemDataRole value);
bool fromString(const std::string_view& str, CoreItemDataRole* value);

} // nx::vms::client::core

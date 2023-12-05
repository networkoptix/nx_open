// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_globals.h"

#include <nx/reflect/enum_string_conversion.h>

namespace nx::vms::client::core {

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(CoreItemDataRole*, Visitor&& visitor)
{
    using CoreItem = nx::reflect::enumeration::Item<CoreItemDataRole>;

    return visitor(
#define IDR_CORE_ITEM(I) CoreItem{CoreItemDataRole::I, #I}

        IDR_CORE_ITEM(ResourceRole),
        IDR_CORE_ITEM(RawResourceRole),
        IDR_CORE_ITEM(LayoutResourceRole),
        IDR_CORE_ITEM(MediaServerResourceRole),

        IDR_CORE_ITEM(ResourceNameRole),
        IDR_CORE_ITEM(ResourceStatusRole),
        IDR_CORE_ITEM(DurationRole),
        IDR_CORE_ITEM(DurationMsRole),
        IDR_CORE_ITEM(PreviewTimeRole),
        IDR_CORE_ITEM(TimestampMsRole),

        IDR_CORE_ITEM(CameraBookmarkRole),
        IDR_CORE_ITEM(BookmarkTagRole),
        IDR_CORE_ITEM(UuidRole),

        IDR_CORE_ITEM(TimestampRole),
        IDR_CORE_ITEM(TimestampTextRole),
        IDR_CORE_ITEM(DescriptionTextRole),
        IDR_CORE_ITEM(ResourceListRole),
        IDR_CORE_ITEM(DisplayedResourceListRole),

        IDR_CORE_ITEM(DefaultNotificationRole),
        IDR_CORE_ITEM(ActivateLinkRole),

        IDR_CORE_ITEM(AnalyticsEngineNameRole),
        IDR_CORE_ITEM(HasExternalBestShotRole),
        IDR_CORE_ITEM(DecorationPathRole),
        IDR_CORE_ITEM(AnalyticsAttributesRole),
        IDR_CORE_ITEM(FlatAttributeListRole),
        IDR_CORE_ITEM(RawAttributeListRole),
        IDR_CORE_ITEM(ObjectTrackIdRole),
        IDR_CORE_ITEM(PreviewStreamSelectionRole),
        IDR_CORE_ITEM(ThumbnailRole),
        IDR_CORE_ITEM(IpAddressRole),

        IDR_CORE_ITEM(CoreItemDataRoleCount)

#undef IDR_CORE_ITEM
        );
}

QHash<int, QByteArray> clientCoreRoleNames()
{
    QHash<int, QByteArray> roles;

    roles[ResourceRole] = "previewResource";
    roles[RawResourceRole] = "resource";
    roles[DecorationPathRole] = "decorationPath";
    roles[ResourceNameRole] = "resourceName";
    roles[ResourceStatusRole] = "resourceStatus";
    roles[PreviewTimeRole] = "previewTime";
    roles[TimestampMsRole] = "timestampMs";
    roles[TimestampTextRole] = "textTimestamp";
    roles[DescriptionTextRole] = "description";
    roles[DurationMsRole] = "durationMs";
    roles[BookmarkTagRole] = "tags";
    roles[DisplayedResourceListRole] = "resourceList";
    roles[FlatAttributeListRole] = "attributes";
    roles[RawAttributeListRole] = "rawAttributes";
    roles[ThumbnailRole] = "thumbnail";
    roles[IpAddressRole] = "ipAddress";
    roles[UuidRole] = "uuid";

    return roles;
}

std::string toString(CoreItemDataRole value)
{
    return nx::reflect::enumeration::toString(value);
}

bool fromString(const std::string_view& str, CoreItemDataRole* value)
{
    return nx::reflect::enumeration::fromString(str, value);
}

} // nx::vms::client::core

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manifest_items.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

bool operator==(const EventType& lh, const EventType& rh)
{
    return lh.id == rh.id;
}

size_t qHash(const EventType& eventType)
{
    return qHash(eventType.id);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ColorItem, (json), ColorItem_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ColorType, (json), ColorType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EnumType, (json), EnumType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AttributeDescription, (json), AttributeDescription_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ItemObject, (json), ItemObject_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ExtendedType, (json), ExtendedType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json), EventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectType, (json), ObjectType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(HiddenExtendedType, (json), HiddenExtendedType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AttributesWithId, (json), AttributesWithId_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Group, (json), Group_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TypeSupportInfo, (json), TypeSupportInfo_Fields, (brief, true))

} // namespace nx::vms::api::analytics

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/flags.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NamedItem
{
    std::string id;
    std::string name;

    bool operator==(const NamedItem& other) const = default;
};
#define NamedItem_Fields (id)(name)
NX_REFLECTION_INSTRUMENT(NamedItem, NamedItem_Fields);

struct NamedItemWithOptionalName
{
    std::string id;
    /**%apidoc[opt] */
    std::string name;

    bool operator==(const NamedItemWithOptionalName& other) const = default;
};
#define NamedItemWithOptionalName_Fields (id)(name)
NX_REFLECTION_INSTRUMENT(NamedItemWithOptionalName,
    NamedItemWithOptionalName_Fields);

struct EnumType: public NamedItem
{
    /**%apidoc[opt] */
    std::optional<std::string> base;
    /**%apidoc[opt] */
    std::vector<std::string> baseItems;
    /**%apidoc[opt] */
    std::vector<std::string> items;
};
#define EnumType_Fields NamedItem_Fields (base)(baseItems)(items)
NX_REFLECTION_INSTRUMENT(EnumType, EnumType_Fields);

struct ColorItem
{
    /**%apidoc[opt] */
    std::string name;
    /**%apidoc[opt] */
    std::string rgb;

    bool operator==(const ColorItem& other) const = default;
};
#define ColorItem_Fields (name)(rgb)
NX_REFLECTION_INSTRUMENT(ColorItem, ColorItem_Fields);

struct ColorType: public NamedItem
{
    std::optional<std::string> base;
    /**%apidoc[opt] */
    std::vector<std::string> baseItems;
    /**%apidoc[opt] */
    std::vector<ColorItem> items;
};
#define ColorType_Fields NamedItem_Fields (base)(baseItems)(items)
NX_REFLECTION_INSTRUMENT(ColorType, ColorType_Fields);

enum class AttributeType
{
    /**%apidoc[unused] */
    undefined,

    /**%apidoc
     * %caption Number
     */
    number,

    /**%apidoc
     * %caption Boolean
     */
    boolean,

    /**%apidoc
     * %caption String
     */
    string,

    /**%apidoc
     * %caption Color
     */
    color,

    /**%apidoc
     * %caption Enum
     */
    enumeration,

    /**%apidoc
     * %caption Object
     */
    object,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(AttributeType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<AttributeType>;
    return visitor(
        Item{AttributeType::undefined, ""},
        Item{AttributeType::number, "Number"},
        Item{AttributeType::boolean, "Boolean"},
        Item{AttributeType::string, "String"},
        Item{AttributeType::color, "Color"},
        Item{AttributeType::enumeration, "Enum"},
        Item{AttributeType::object, "Object"});
}

struct AttributeDescriptionCommon
{
    /**%apidoc[opt] */
    std::string name;
    std::optional<AttributeType> type; /**< Can be omitted for Enum and Color attributes. */
    std::optional<std::string> subtype; /**< One of: empty, a Name, "integer" or "float" for Numbers. */
    std::optional<std::string> unit; /**< Only for Number. Can be empty. */

    /**%apidoc:integer */
    std::optional<double> minValue; /**< Only for Number. */

    /**%apidoc:integer */
    std::optional<double> maxValue; /**< Only for Number. */

    std::optional<std::string> attributeList; /**< If not empty, all other fields are ignored. */

    /**
     * Condition string that defines whether this Attribute makes sense for the Object or Event
     * Type depending on values of the other Attributes. Uses the same syntax as in the Object
     * Search panel.
     */
    std::optional<std::string> condition;

    AttributeDescriptionCommon()
    {
    }

    bool operator==(const AttributeDescriptionCommon& other) const = default;
};

#define AttributeDescriptionCommon_Fields \
    (name)\
    (type)\
    (subtype)\
    (unit)\
    (minValue)\
    (maxValue)\
    (attributeList)\
    (condition)

struct DependentAttributeDescription : AttributeDescriptionCommon
{
    std::optional<std::vector<std::string>> items; /**< Only for Enums. */

    bool operator==(const DependentAttributeDescription& other) const = default;
};
#define DependentAttributeDescription_Fields \
    AttributeDescriptionCommon_Fields \
    (items)

struct ItemObject
{
    /**%apidoc[opt] */
    std::string value;
    /**%apidoc[opt] */
    std::vector<DependentAttributeDescription> dependentAttributes;

    bool operator==(const ItemObject& other) const = default;
};
#define ItemObject_Fields \
    (value) \
    (dependentAttributes)

using Item = std::variant<ItemObject, std::string>;

struct AttributeDescription : AttributeDescriptionCommon
{
    std::optional<std::vector<std::variant<ItemObject, std::string>>> items; /**< Only for Enums. */

    bool operator==(const AttributeDescription& other) const = default;

    AttributeDescription()
    {
    }

    AttributeDescription(const DependentAttributeDescription& dep)
        : AttributeDescriptionCommon(dep)
    {
        if (dep.items)
        {
            std::vector<Item> convertedItems;
            for (const auto& item : *dep.items)
                convertedItems.emplace_back(item);
            items = std::move(convertedItems);
        }
    }
};

#define AttributeDescription_Fields \
    AttributeDescriptionCommon_Fields \
    (items)

NX_REFLECTION_INSTRUMENT(AttributeDescriptionCommon, AttributeDescriptionCommon_Fields);
NX_REFLECTION_INSTRUMENT(ItemObject, ItemObject_Fields);
NX_REFLECTION_INSTRUMENT(DependentAttributeDescription, DependentAttributeDescription_Fields);
NX_REFLECTION_INSTRUMENT(AttributeDescription, AttributeDescription_Fields);

struct ExtendedTypeCommon
{
    /**%apidoc[opt] */
    std::string icon; //< internal id or empty.
    /**%apidoc[opt] */
    std::optional<std::string> base;
    /**%apidoc[opt] */
    std::vector<std::string> omittedBaseAttributes;
    /**%apidoc[opt] */
    std::vector<AttributeDescription> attributes;

    bool operator==(const ExtendedTypeCommon& other) const = default;
};
#define ExtendedTypeCommon_Fields (icon)(base)(omittedBaseAttributes)(attributes)
NX_REFLECTION_INSTRUMENT(ExtendedTypeCommon, ExtendedTypeCommon_Fields);

struct ExtendedType: public NamedItem, public ExtendedTypeCommon
{
    bool operator==(const ExtendedType& other) const = default;
};
#define ExtendedType_Fields NamedItem_Fields ExtendedTypeCommon_Fields
NX_REFLECTION_INSTRUMENT(ExtendedType, ExtendedType_Fields);

struct ExtendedTypeWithOptionalName: public NamedItemWithOptionalName,
    public ExtendedTypeCommon
{
    bool operator==(const ExtendedTypeWithOptionalName& other) const = default;
};
#define ExtendedTypeWithOptionalName_Fields NamedItemWithOptionalName_Fields ExtendedTypeCommon_Fields
NX_REFLECTION_INSTRUMENT(ExtendedTypeWithOptionalName,
    ExtendedTypeWithOptionalName_Fields);

/** See the documentation in manifests.md. */
enum class EventTypeFlag
{
    noFlags = 0,
    stateDependent = 1 << 0,
    regionDependent = 1 << 1,
    hidden = 1 << 2,
    useTrackBestShotAsPreview = 1 << 3
};
Q_DECLARE_FLAGS(EventTypeFlags, EventTypeFlag)

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(EventTypeFlag*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<EventTypeFlag>;
    return visitor(
        Item{EventTypeFlag::noFlags, ""},
        Item{EventTypeFlag::noFlags, "noFlags"},
        Item{EventTypeFlag::stateDependent, "stateDependent"},
        Item{EventTypeFlag::regionDependent, "regionDependent"},
        Item{EventTypeFlag::hidden, "hidden"},
        Item{EventTypeFlag::useTrackBestShotAsPreview, "useTrackBestShotAsPreview"});
}

/**
 * Description of the Analytics Event type.
 */
struct AnalyticsEventType: public ExtendedType
{
    /**%apidoc[opt] */
    EventTypeFlags flags = EventTypeFlag::noFlags;
    /**%apidoc[opt] */
    std::string groupId;
    /**%apidoc[opt] */
    std::string provider;

    bool isStateful() const noexcept { return flags.testFlag(EventTypeFlag::stateDependent); }
};
#define AnalyticsEventType_Fields ExtendedType_Fields (flags)(groupId)(provider)
NX_REFLECTION_INSTRUMENT(AnalyticsEventType, AnalyticsEventType_Fields);
size_t NX_VMS_API qHash(const AnalyticsEventType& eventType);

enum class ObjectTypeFlag
{
    noFlags = 0,
    hiddenDerivedType = 1 << 0,
    nonIndexable = 1 << 1,
    liveOnly = 1 << 2,
};
Q_DECLARE_FLAGS(ObjectTypeFlags, ObjectTypeFlag)

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ObjectTypeFlag*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ObjectTypeFlag>;
    return visitor(
        Item{ObjectTypeFlag::noFlags, ""},
        Item{ObjectTypeFlag::noFlags, "noFlags"},
        Item{ObjectTypeFlag::hiddenDerivedType, "hiddenDerivedType"},
        Item{ObjectTypeFlag::nonIndexable, "nonIndexable"},
        Item{ObjectTypeFlag::liveOnly, "liveOnly"});
}

/**
 * Description of the Analytics Object Type.
 */
struct ObjectType: public ExtendedTypeWithOptionalName
{
    /**%apidoc[opt] */
    std::string provider;
    /**%apidoc[opt] */
    ObjectTypeFlags flags;

    bool operator==(const ObjectType& other) const = default;
};
#define ObjectType_Fields ExtendedTypeWithOptionalName_Fields (provider)(flags)
NX_REFLECTION_INSTRUMENT(ObjectType, ObjectType_Fields);

struct HiddenExtendedType
{
    std::string id;
    std::vector<AttributeDescription> attributes;

    bool operator==(const HiddenExtendedType& other) const = default;
};
#define HiddenExtendedType_Fields (id)(attributes)
NX_REFLECTION_INSTRUMENT(HiddenExtendedType, HiddenExtendedType_Fields);

struct AttributesWithId
{
    std::string id;
    std::vector<AttributeDescription> attributes;

    bool operator==(const AttributesWithId& other) const = default;
};
#define AttributesWithId_Fields (id)(attributes)
NX_REFLECTION_INSTRUMENT(AttributesWithId, AttributesWithId_Fields)

/**
 * Named group which is referenced from a "groupId" attribute of other types to group them.
 */
struct Group: NamedItem
{
};
#define Group_Fields NamedItem_Fields
NX_REFLECTION_INSTRUMENT(Group, Group_Fields);

struct TypeSupportInfo
{
    /**%apidoc[opt] */
    std::string eventTypeId; //< eventTypeId and objectTypeId can't be non-empty at the same time.
    /**%apidoc[opt] */
    std::string objectTypeId;
    /**%apidoc[opt] */
    std::vector<std::string> attributes;

    bool operator==(const TypeSupportInfo& other) const = default;
};

#define TypeSupportInfo_Fields \
    (eventTypeId) \
    (objectTypeId) \
    (attributes)
NX_REFLECTION_INSTRUMENT(TypeSupportInfo, TypeSupportInfo_Fields);

Q_DECLARE_OPERATORS_FOR_FLAGS(EventTypeFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(ObjectTypeFlags)

bool NX_VMS_API operator==(const AnalyticsEventType& lh, const AnalyticsEventType& rh);

QN_FUSION_DECLARE_FUNCTIONS(ColorItem, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ColorType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EnumType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ItemObject, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(AttributeDescription, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ExtendedType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ObjectType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(HiddenExtendedType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(AttributesWithId, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(Group, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(TypeSupportInfo, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EventTypeFlag, (lexical),
    NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EventTypeFlags, (lexical),
    NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::ObjectTypeFlag, (lexical),
    NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::ObjectTypeFlags, (lexical),
    NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::AttributeType, (lexical),
    NX_VMS_API)

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NamedItem
{
    QString id;
    QString name;
};
#define NamedItem_Fields (id)(name)
NX_REFLECTION_INSTRUMENT(NamedItem, NamedItem_Fields);

struct EnumType: public NamedItem
{
    std::optional<QString> base;
    std::vector<QString> baseItems;
    std::vector<QString> items;
};
#define EnumType_Fields NamedItem_Fields (base)(baseItems)(items)
NX_REFLECTION_INSTRUMENT(EnumType, EnumType_Fields);

struct ColorItem
{
    QString name;
    QString rgb;

    bool operator==(const ColorItem& other) const = default;
};
#define ColorItem_Fields (name)(rgb)
NX_REFLECTION_INSTRUMENT(ColorItem, ColorItem_Fields);

struct ColorType: public NamedItem
{
    std::optional<QString> base;
    std::vector<QString> baseItems;
    std::vector<ColorItem> items;
};
#define ColorType_Fields NamedItem_Fields (base)(baseItems)(items)
NX_REFLECTION_INSTRUMENT(ColorType, ColorType_Fields);

enum class AttributeType
{
    undefined,
    number,
    boolean,
    string,
    color,
    enumeration,
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

struct AttributeDescription;

struct ItemObject
{
    QString value;
    std::vector<AttributeDescription> dependentAttributes;

    bool operator==(const ItemObject& other) const = default;
};
#define ItemObject_Fields \
    (value) \
    (dependentAttributes)

using Item = std::variant<ItemObject, QString>;

struct AttributeDescription
{
    QString name;
    std::optional<AttributeType> type; /**< Can be omitted for Enum and Color attributes. */
    QString subtype; /**< One of: empty, a Name, "integer" or "float" for Numbers. */
    std::optional<std::vector<Item>> items; /**< Only for Enums. */
    QString unit; /**< Only for Number. Can be empty. */
    std::optional<double> minValue; /**< Only for Number. */
    std::optional<double> maxValue; /**< Only for Number. */
    QString attributeList; /**< If not empty, all other fields are ignored. */

    /**
     * Condition string that defines whether this Attribute makes sense for the Object or Event
     * Type depending on values of the other Attributes. Uses the same syntax as in the Object
     * Search panel.
     */
    QString condition;

    bool operator==(const AttributeDescription& other) const = default;
};
#define AttributeDescription_Fields \
    (name)\
    (type)\
    (subtype)\
    (items)\
    (unit)\
    (minValue)\
    (maxValue)\
    (attributeList)\
    (condition)

NX_REFLECTION_INSTRUMENT(ItemObject, ItemObject_Fields);

NX_REFLECTION_INSTRUMENT(AttributeDescription, AttributeDescription_Fields);

struct ExtendedType: public NamedItem
{
    QString icon; //< internal id or empty.
    std::optional<QString> base;
    std::vector<QString> omittedBaseAttributes;
    std::vector<AttributeDescription> attributes;
};
#define ExtendedType_Fields NamedItem_Fields (icon)(base)(omittedBaseAttributes)(attributes)
NX_REFLECTION_INSTRUMENT(ExtendedType, ExtendedType_Fields);

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
struct EventType: public ExtendedType
{
    EventTypeFlags flags = EventTypeFlag::noFlags;
    QString groupId;
    QString provider = "";

    bool isStateful() const noexcept { return flags.testFlag(EventTypeFlag::stateDependent); }
};
#define EventType_Fields ExtendedType_Fields (flags)(groupId)(provider)
NX_REFLECTION_INSTRUMENT(EventType, EventType_Fields);
size_t NX_VMS_API qHash(const EventType& eventType);

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
struct ObjectType: public ExtendedType
{
    QString provider;
    ObjectTypeFlags flags;
};
#define ObjectType_Fields ExtendedType_Fields (provider)(flags)
NX_REFLECTION_INSTRUMENT(ObjectType, ObjectType_Fields);

struct HiddenExtendedType
{
    QString id;
    std::vector<AttributeDescription> attributes;
};
#define HiddenExtendedType_Fields (id)(attributes)
NX_REFLECTION_INSTRUMENT(HiddenExtendedType, HiddenExtendedType_Fields);

struct AttributeList
{
    QString id;
    std::vector<AttributeDescription> attributes;

    bool operator==(const AttributeList& other) const = default;
};
#define AttributeList_Fields (id)(attributes)
NX_REFLECTION_INSTRUMENT(AttributeList, AttributeList_Fields)

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
    QString eventTypeId; //< eventTypeId and objectTypeId can't be non-empty at the same time.
    QString objectTypeId;
    std::vector<QString> attributes;

    bool operator==(const TypeSupportInfo& other) const = default;
};

#define TypeSupportInfo_Fields \
    (eventTypeId) \
    (objectTypeId) \
    (attributes)
NX_REFLECTION_INSTRUMENT(TypeSupportInfo, TypeSupportInfo_Fields);

Q_DECLARE_OPERATORS_FOR_FLAGS(EventTypeFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(ObjectTypeFlags)

bool NX_VMS_API operator==(const EventType& lh, const EventType& rh);

QN_FUSION_DECLARE_FUNCTIONS(ColorItem, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ColorType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EnumType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ItemObject, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(AttributeDescription, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ExtendedType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EventType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ObjectType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(HiddenExtendedType, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(AttributeList, (json), NX_VMS_API)
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

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <type_traits>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/manifest_items.h>

#include "descriptors_fwd.h"

namespace nx::vms::api::analytics {

struct DescriptorScope
{
    QnUuid engineId;
    mutable QString groupId;
    mutable QString provider;
    mutable bool hasTypeEverBeenSupportedInThisScope = false;

    bool operator==(const DescriptorScope& other) const = default;

    bool operator<(const DescriptorScope& other) const
    {
        return engineId < other.engineId;
    }

    bool isNull() const
    {
        return engineId.isNull() && groupId.isEmpty() && provider.isEmpty();
    }
};
#define nx_vms_api_analytics_DescriptorScope_Fields (engineId)\
    (groupId)\
    (provider)\
    (hasTypeEverBeenSupportedInThisScope)
NX_REFLECTION_INSTRUMENT(DescriptorScope, nx_vms_api_analytics_DescriptorScope_Fields);

template<typename T, typename = void>
struct hasGroupId: std::false_type {};

template<typename T>
struct hasGroupId<T, std::void_t<decltype(std::declval<T>().groupId)>>: std::true_type {};

template<typename T, typename = void>
struct hasProvider: std::false_type {};

template<typename T>
struct hasProvider<T, std::void_t<decltype(std::declval<T>().provider)>>: std::true_type {};

template<typename T, typename = void>
struct hasScopes: std::false_type {};

template<typename T>
struct hasScopes<T, std::void_t<decltype(std::declval<T>().scopes)>>: std::true_type {};

template<typename T>
DescriptorScope scopeFromItem(QnUuid engineId, T item)
{
    DescriptorScope scope;
    if (engineId.isNull())
        return DescriptorScope();

    scope.engineId = std::move(engineId);
    if constexpr (hasGroupId<T>::value)
        scope.groupId = std::move(item.groupId);

    if constexpr (hasProvider<T>::value)
        scope.provider = std::move(item.provider);

    return scope;
}

struct BaseDescriptor
{
    QString id;
    QString name;

    BaseDescriptor() = default;
    BaseDescriptor(NamedItem namedItem):
        id(std::move(namedItem.id)),
        name(std::move(namedItem.name))
    {
    }

    BaseDescriptor(QString id, QString name):
        id(std::move(id)),
        name(std::move(name))
    {
    }

    bool operator==(const BaseDescriptor& other) const = default;

    QString getId() const { return id; }
};
#define nx_vms_api_analytics_BaseDescriptor_Fields (id)(name)
NX_REFLECTION_INSTRUMENT(BaseDescriptor, nx_vms_api_analytics_BaseDescriptor_Fields);

struct BaseScopedDescriptor: BaseDescriptor
{
    BaseScopedDescriptor() = default;
    BaseScopedDescriptor(DescriptorScope scope, NamedItem namedItem):
        BaseDescriptor(std::move(namedItem))
    {
        if (!scope.isNull())
            scopes.insert(std::move(scope));
    }

    BaseScopedDescriptor(DescriptorScope scope, QString id, QString name):
        BaseDescriptor(std::move(id), std::move(name))
    {
        if (!scope.isNull())
            scopes.insert(std::move(scope));
    }

    bool operator==(const BaseScopedDescriptor& other) const = default;

    std::set<DescriptorScope> scopes;
};
#define nx_vms_api_analytics_BaseScopedDescriptor_Fields \
    nx_vms_api_analytics_BaseDescriptor_Fields \
    (scopes)
NX_REFLECTION_INSTRUMENT(BaseScopedDescriptor, nx_vms_api_analytics_BaseScopedDescriptor_Fields);

struct PluginDescriptor: BaseDescriptor
{
    PluginDescriptor() = default;
    PluginDescriptor(QString id, QString name):
        BaseDescriptor(std::move(id), std::move(name))
    {
    }

    bool operator==(const PluginDescriptor& other) const = default;
};
#define nx_vms_api_analyitcs_PluginDescriptor_Fields nx_vms_api_analytics_BaseDescriptor_Fields
NX_REFLECTION_INSTRUMENT(PluginDescriptor, nx_vms_api_analyitcs_PluginDescriptor_Fields);

struct EngineDescriptor
{
    QnUuid id;
    QString name;
    QString pluginId;
    EngineManifest::Capabilities capabilities;

    EngineDescriptor() = default;
    EngineDescriptor(
        QnUuid id,
        QString name,
        QString pluginId,
        const EngineManifest& engineManifest)
        :
        id(std::move(id)),
        name(std::move(name)),
        pluginId(std::move(pluginId)),
        capabilities(engineManifest.capabilities)
    {
    }

    bool operator==(const EngineDescriptor& other) const = default;

    QnUuid getId() const { return id; }
};
#define nx_vms_api_analytics_EngineDescriptor_Fields (id)(name)(pluginId)(capabilities)
NX_REFLECTION_INSTRUMENT(EngineDescriptor, nx_vms_api_analytics_EngineDescriptor_Fields);

struct GroupDescriptor: BaseScopedDescriptor
{
    GroupDescriptor() = default;
    GroupDescriptor(QnUuid engineId, const Group& group):
        BaseScopedDescriptor(scopeFromItem(engineId, group), group)
    {
    }

    bool operator==(const GroupDescriptor& other) const = default;
};
#define nx_vms_api_analyitcs_GroupDescriptor_Fields \
    nx_vms_api_analytics_BaseScopedDescriptor_Fields
NX_REFLECTION_INSTRUMENT(GroupDescriptor, nx_vms_api_analyitcs_GroupDescriptor_Fields);

struct ExtendedScopedDescriptor: public BaseScopedDescriptor
{
    ExtendedScopedDescriptor() = default;
    ExtendedScopedDescriptor(DescriptorScope scope, ExtendedType extendedType):
        BaseScopedDescriptor(std::move(scope), extendedType),
        icon(std::move(extendedType.icon)),
        base(std::move(extendedType.base)),
        omittedBaseAttributes(std::move(extendedType.omittedBaseAttributes)),
        attributes(std::move(extendedType.attributes))
    {
    }

    bool operator==(const ExtendedScopedDescriptor& other) const = default;

    QString icon;
    std::optional<QString> base;
    std::vector<QString> omittedBaseAttributes;
    std::vector<AttributeDescription> attributes;
    std::map<QString /*attributeName*/, std::set<EngineId>> attributeSupportInfo;
};
#define nx_vms_api_analytics_ExtendedScopedDescriptor_Fields \
    nx_vms_api_analytics_BaseScopedDescriptor_Fields \
    (icon) \
    (base) \
    (omittedBaseAttributes) \
    (attributes) \
    (attributeSupportInfo)
NX_REFLECTION_INSTRUMENT(ExtendedScopedDescriptor,
    nx_vms_api_analytics_ExtendedScopedDescriptor_Fields);

struct EventTypeDescriptor: public ExtendedScopedDescriptor
{
    EventTypeDescriptor() = default;
    EventTypeDescriptor(QnUuid engineId, const EventType& eventType):
        ExtendedScopedDescriptor(scopeFromItem(engineId, eventType), eventType),
        flags(eventType.flags)
    {
    }

    bool operator==(const EventTypeDescriptor& other) const = default;

    EventTypeFlags flags;
    bool hasEverBeenSupported = false;

    /** See ObjectType::isSupposedToMimicBaseType(). */
    bool isSupposedToMimicBaseType() const { return false; }

    bool isHidden() const { return flags.testFlag(EventTypeFlag::hidden); }
};
#define nx_vms_api_analyitcs_EventTypeDescriptor_Fields \
    nx_vms_api_analytics_ExtendedScopedDescriptor_Fields \
    (flags) \
    (hasEverBeenSupported)
NX_REFLECTION_INSTRUMENT(EventTypeDescriptor, nx_vms_api_analyitcs_EventTypeDescriptor_Fields);

struct ObjectTypeDescriptor: public ExtendedScopedDescriptor
{
    ObjectTypeDescriptor() = default;
    ObjectTypeDescriptor(QnUuid engineId, const ObjectType& objectType):
        ExtendedScopedDescriptor(scopeFromItem(engineId, objectType), objectType),
        flags(objectType.flags)
    {
    }

    bool operator==(const ObjectTypeDescriptor& other) const = default;

    ObjectTypeFlags flags;
    bool hasEverBeenSupported = false;

    /**
     * Some Object Types aren't visible anywhere in the GUI (hidden), but behave like mixins for
     * its base Types (mimicking them). E.g its attributes become a part of the base Object Type
     * attribute set.
     */
    bool isSupposedToMimicBaseType() const
    {
        return flags.testFlag(ObjectTypeFlag::hiddenDerivedType);
    }

    bool isHidden() const { return isSupposedToMimicBaseType(); }
};
#define nx_vms_api_analyitcs_ObjectTypeDescriptor_Fields \
    nx_vms_api_analytics_ExtendedScopedDescriptor_Fields \
    (flags) \
    (hasEverBeenSupported)
NX_REFLECTION_INSTRUMENT(ObjectTypeDescriptor, nx_vms_api_analyitcs_ObjectTypeDescriptor_Fields);

struct ActionTypeDescriptor: BaseDescriptor
{
    ActionTypeDescriptor() = default;
    ActionTypeDescriptor(QnUuid /*engineId*/, EngineManifest::ObjectAction actionType):
        BaseDescriptor(
            actionType.id,
            actionType.name),
        supportedObjectTypeIds(std::move(actionType.supportedObjectTypeIds)),
        parametersModel(std::move(actionType.parametersModel)),
        requirements(std::move(actionType.requirements))
    {
    }

    bool operator==(const ActionTypeDescriptor& other) const = default;

    QList<QString> supportedObjectTypeIds;
    QJsonObject parametersModel;
    analytics::EngineManifest::ObjectAction::Requirements requirements;
};
#define nx_vms_api_analytics_ActionTypeDescriptor_Fields \
    nx_vms_api_analytics_BaseDescriptor_Fields \
    (supportedObjectTypeIds) \
    (parametersModel) \
    (requirements)
NX_REFLECTION_INSTRUMENT(ActionTypeDescriptor, nx_vms_api_analytics_ActionTypeDescriptor_Fields);

struct EnumTypeDescriptor: public BaseDescriptor
{
    EnumTypeDescriptor() = default;
    EnumTypeDescriptor(QnUuid /*engineId*/, EnumType enumType):
        BaseDescriptor(enumType),
        base(std::move(enumType.base)),
        baseItems(std::move(enumType.baseItems)),
        items(std::move(enumType.items))
    {
    }

    bool operator==(const EnumTypeDescriptor& other) const = default;

    std::optional<QString> base;
    std::vector<QString> baseItems;
    std::vector<QString> items;
};
#define nx_vms_api_analytics_EnumTypeDescriptor_Fields \
    nx_vms_api_analytics_BaseDescriptor_Fields \
    (base) \
    (baseItems) \
    (items)
NX_REFLECTION_INSTRUMENT(EnumTypeDescriptor, nx_vms_api_analytics_EnumTypeDescriptor_Fields);

struct ColorTypeDescriptor: public BaseDescriptor
{
    ColorTypeDescriptor() = default;
    ColorTypeDescriptor(QnUuid /*engineId*/, ColorType colorType):
        BaseDescriptor(colorType),
        base(std::move(colorType.base)),
        baseItems(std::move(colorType.baseItems)),
        items(std::move(colorType.items))
    {
    }

    bool operator==(const ColorTypeDescriptor& other) const = default;

    std::optional<QString> base;
    std::vector<QString> baseItems;
    std::vector<ColorItem> items;
};
#define nx_vms_api_analytics_ColorTypeDescriptor_Fields \
    nx_vms_api_analytics_BaseDescriptor_Fields \
    (base) \
    (baseItems) \
    (items)
NX_REFLECTION_INSTRUMENT(ColorTypeDescriptor, nx_vms_api_analytics_ColorTypeDescriptor_Fields);

// Client and web api uses group ids in the same filter scenarios as event and object type ids.
static_assert(std::is_same<GroupId, EventTypeId>::value);
static_assert(std::is_same<GroupId, ObjectTypeId>::value);

using PluginDescriptorMap = std::map<PluginId, PluginDescriptor>;
using EngineDescriptorMap = std::map<EngineId, EngineDescriptor>;
using GroupDescriptorMap = std::map<GroupId, GroupDescriptor>;
using EventTypeDescriptorMap = std::map<EventTypeId, EventTypeDescriptor>;
using ObjectTypeDescriptorMap = std::map<ObjectTypeId, ObjectTypeDescriptor>;

using EnumTypeDescriptorMap = std::map<EnumTypeId, EnumTypeDescriptor>;
using ColorTypeDescriptorMap = std::map<ColorTypeId, ColorTypeDescriptor>;

using AttributeListMap = std::map<AttributeListId, AttributesWithId>;

using ScopedEventTypeIds = std::map<EngineId, std::map<GroupId, std::set<EventTypeId>>>;
using ScopedObjectTypeIds = std::map<EngineId, std::map<GroupId, std::set<ObjectTypeId>>>;
using ScopedEntityTypeIds = std::map<EngineId, std::map<GroupId, std::set<QString>>>;

using ActionTypeDescriptorMap = std::map<EngineId, std::map<ActionTypeId, ActionTypeDescriptor>>;

struct NX_VMS_API Descriptors
{
    PluginDescriptorMap pluginDescriptors;
    EngineDescriptorMap engineDescriptors;
    GroupDescriptorMap groupDescriptors;
    EventTypeDescriptorMap eventTypeDescriptors;
    ObjectTypeDescriptorMap objectTypeDescriptors;
    EnumTypeDescriptorMap enumTypeDescriptors;
    ColorTypeDescriptorMap colorTypeDescriptors;
    AttributeListMap attributeListDescriptors;

    void merge(Descriptors descriptors);

    bool isEmpty() const;
    bool operator==(const Descriptors& other) const = default;
};

#define nx_vms_api_analytics_Descriptors_Fields \
    (pluginDescriptors) \
    (engineDescriptors) \
    (groupDescriptors) \
    (eventTypeDescriptors) \
    (objectTypeDescriptors) \
    (enumTypeDescriptors) \
    (colorTypeDescriptors) \
    (attributeListDescriptors)
NX_REFLECTION_INSTRUMENT(Descriptors, nx_vms_api_analytics_Descriptors_Fields);

QN_FUSION_DECLARE_FUNCTIONS(Descriptors, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(DescriptorScope, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(BaseDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(BaseScopedDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ExtendedScopedDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(PluginDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EngineDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(GroupDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EventTypeDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ObjectTypeDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ActionTypeDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EnumTypeDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ColorTypeDescriptor, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics

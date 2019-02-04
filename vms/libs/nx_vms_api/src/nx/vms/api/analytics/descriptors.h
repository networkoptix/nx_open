#pragma once

#include <set>
#include <type_traits>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms::api::analytics {

struct DescriptorScope
{
    QnUuid engineId;
    QString groupId;

    bool operator<(const DescriptorScope& other) const
    {
        // Two paths with the same pluginId and different groupId are not allowed because
        // only single Engine per Plugin is supported. In the future, when we support multiple
        // Engines per Plugin, this have to change.
        return engineId < other.engineId;
    }
};
#define nx_vms_api_analytics_DescriptorScope_Fields (engineId)(groupId)

template<typename T, typename = void>
struct hasGroupId: std::false_type {};

template<typename T>
struct hasGroupId<T, std::void_t<decltype(std::declval<T>().groupId)>>: std::true_type {};

template<typename T>
DescriptorScope scopeFromItem(QnUuid engineId, T item)
{
    DescriptorScope scope;
    scope.engineId = std::move(engineId);
    if constexpr (hasGroupId<T>::value)
        scope.groupId = std::move(item.groupId);

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

    QString getId() const { return id; }
};
#define nx_vms_api_analytics_BaseDescriptor_Fields (id)(name)

struct BaseScopedDescriptor: BaseDescriptor
{
    BaseScopedDescriptor() = default;
    BaseScopedDescriptor(DescriptorScope scope, NamedItem namedItem):
        BaseDescriptor(std::move(namedItem))
    {
        scopes.insert(std::move(scope));
    }

    BaseScopedDescriptor(DescriptorScope scope, QString id, QString name):
        BaseDescriptor(std::move(id), std::move(name))
    {
        scopes.insert(std::move(scope));
    }

    std::set<DescriptorScope> scopes;
};
#define nx_vms_api_analytics_BaseScopedDescriptor_Fields \
    nx_vms_api_analytics_BaseDescriptor_Fields \
    (scopes)

struct PluginDescriptor: BaseDescriptor
{
    PluginDescriptor() = default;
    PluginDescriptor(QString id, QString name):
        BaseDescriptor(std::move(id), std::move(name))
    {
    }
};
#define nx_vms_api_analyitcs_PluginDescriptor_Fields nx_vms_api_analytics_BaseDescriptor_Fields

struct EngineDescriptor
{
    QnUuid id;
    QString name;
    QString pluginId;

    EngineDescriptor() = default;
    EngineDescriptor(QnUuid id, QString name, QString pluginId):
        id(std::move(id)),
        name(std::move(name)),
        pluginId(std::move(pluginId))
    {
    }

    QnUuid getId() const { return id; }
};
#define nx_vms_api_analytics_EngineDescriptor_Fields (id)(name)(pluginId)

struct GroupDescriptor: BaseScopedDescriptor
{
    GroupDescriptor() = default;
    GroupDescriptor(QnUuid engineId, const Group& group):
        BaseScopedDescriptor(scopeFromItem(engineId, group), group)
    {
    }
};
#define nx_vms_api_analyitcs_GroupDescriptor_Fields \
    nx_vms_api_analytics_BaseScopedDescriptor_Fields

struct EventTypeDescriptor: BaseScopedDescriptor
{
    EventTypeDescriptor() = default;
    EventTypeDescriptor(QnUuid engineId, const EventType& eventType):
        BaseScopedDescriptor(scopeFromItem(engineId, eventType), eventType),
        flags(eventType.flags)
    {
    }

    EventTypeFlags flags;

    bool isHidden() const { return flags.testFlag(EventTypeFlag::hidden); }
};
#define nx_vms_api_analyitcs_EventTypeDescriptor_Fields \
    nx_vms_api_analytics_BaseScopedDescriptor_Fields \
    (flags)

struct ObjectTypeDescriptor: BaseScopedDescriptor
{
    ObjectTypeDescriptor() = default;
    ObjectTypeDescriptor(QnUuid engineId, const ObjectType& objectType):
        BaseScopedDescriptor(scopeFromItem(engineId, objectType), objectType)
    {
    }
};
#define nx_vms_api_analyitcs_ObjectTypeDescriptor_Fields \
    nx_vms_api_analytics_BaseScopedDescriptor_Fields

struct ActionTypeDescriptor: BaseDescriptor
{
    ActionTypeDescriptor() = default;
    ActionTypeDescriptor(QnUuid /*engineId*/, EngineManifest::ObjectAction actionType):
        BaseDescriptor(
            actionType.id,
            actionType.name),
        supportedObjectTypeIds(std::move(actionType.supportedObjectTypeIds)),
        parametersModel(std::move(actionType.parametersModel))
    {
    }

    QList<QString> supportedObjectTypeIds;
    QJsonObject parametersModel;
};
#define nx_vms_api_analytics_ActionTypeDescriptor_Fields \
    nx_vms_api_analytics_BaseDescriptor_Fields \
    (supportedObjectTypeIds) \
    (parametersModel)

struct DeviceDescriptor
{
    QnUuid id; //< Device id.
    std::set<QnUuid> compatibleEngineIds;
    std::map<QnUuid, std::set<QString>> supportedEventTypeIds;
    std::map<QnUuid, std::set<QString>> supportedObjectTypeIds;

    QnUuid getId() const { return id; };
};
#define nx_vms_api_analytics_DeviceDescriptor_Fields \
    (id) \
    (compatibleEngineIds) \
    (supportedEventTypeIds) \
    (supportedObjectTypeIds)

QN_FUSION_DECLARE_FUNCTIONS(DescriptorScope, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(BaseDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(BaseScopedDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(PluginDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EngineDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(GroupDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EventTypeDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ObjectTypeDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ActionTypeDescriptor, (json)(eq), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(DeviceDescriptor, (json)(eq), NX_VMS_API)

} // namespace nx::vms::api::analytics

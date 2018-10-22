#pragma once

#include <set>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/analytics/manifest_items.h>

namespace nx::vms::api::analytics {

struct PluginDescriptor
{
    QString id;
    QString name;

    QString getId() const { return id; };
};
#define nx_vms_api_analyitcs_PluginDescriptor_Fields (id)(name)

struct GroupDescriptor
{
    QString id;
    QString name;

    QString getId() const { return id; };
};
#define nx_vms_api_analyitcs_GroupDescriptor_Fields (id)(name)

struct EventTypeDescriptor
{
    std::set<QString> pluginIds;
    std::set<QString> groupIds;
    EventType item;

    QString getId() const { return item.id; };
};
#define  nx_vms_api_analyitcs_EventTypeDescriptor_Fields (pluginIds)(groupIds)(item)

struct ObjectTypeDescriptor
{
    std::set<QString> pluginIds;
    ObjectType item;

    QString getId() const { return item.id; };
};
#define nx_vms_api_analyitcs_ObjectTypeDescriptor_Fields (pluginIds)(item)

QN_FUSION_DECLARE_FUNCTIONS(PluginDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(GroupDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EventTypeDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ObjectTypeDescriptor, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics

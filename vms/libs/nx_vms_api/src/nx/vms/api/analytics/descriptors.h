#pragma once

#include <set>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms::api::analytics {

struct HierarchyPath
{
    QString pluginId;
    QString groupId;

    bool operator<(const HierarchyPath& other) const
    {
        return pluginId < other.pluginId;
    }
};
#define nx_vms_api_analytics_HierarchyPath_Fields (pluginId)(groupId)

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
    std::set<HierarchyPath> paths;
    EventType item;

    QString getId() const { return item.id; };

    bool hasGroup(const QString& groupId) const
    {
        for (const auto& path: paths)
        {
            if (path.groupId == groupId)
                return true;
        }

        return false;
    };

    bool isSupportedByPlugin(const QString& pluginId)
    {
        return paths.find({pluginId, QString()}) != paths.cend();
    }
};
#define  nx_vms_api_analyitcs_EventTypeDescriptor_Fields (paths)(item)

struct ObjectTypeDescriptor
{
    std::set<HierarchyPath> paths;
    ObjectType item;

    QString getId() const { return item.id; };
};
#define nx_vms_api_analyitcs_ObjectTypeDescriptor_Fields (paths)(item)

struct ActionTypeDescriptor
{
    std::set<HierarchyPath> paths;
    EngineManifest::ObjectAction item;

    QString getId() const { return item.id; };
};
#define nx_vms_api_analytics_ActionTypeDescriptor_Fields (paths)(item)

QN_FUSION_DECLARE_FUNCTIONS(HierarchyPath, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(PluginDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(GroupDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EventTypeDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ObjectTypeDescriptor, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(ActionTypeDescriptor, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics

#include "abstract_permissions_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

QnAbstractPermissionsModel::QnAbstractPermissionsModel(QObject* parent) :
    QObject(parent)
{

}

QnAbstractPermissionsModel::~QnAbstractPermissionsModel()
{

}

QList<QnAbstractPermissionsModel::Filter> QnAbstractPermissionsModel::allFilters()
{
    return QList<Filter>() << CamerasFilter << LayoutsFilter << ServersFilter;
}

QnResourceList QnAbstractPermissionsModel::filteredResources(Filter filter, const QnResourceList& source)
{
    switch (filter)
    {
    case QnAbstractPermissionsModel::CamerasFilter:
        return source.filtered<QnResource>([](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::desktop_camera))
                return false;
            return (resource->flags().testFlag(Qn::web_page) || resource->flags().testFlag(Qn::server_live_cam));
        });

    case QnAbstractPermissionsModel::LayoutsFilter:
        return source.filtered<QnLayoutResource>([](const QnLayoutResourcePtr& layout)
        {
            return layout->isGlobal() && !layout->isFile();
        });

    case QnAbstractPermissionsModel::ServersFilter:
        return source.filtered<QnMediaServerResource>([](const QnMediaServerResourcePtr& server)
        {
            return !QnMediaServerResource::isFakeServer(server);
        });
    }
    return QnResourceList();
}

QSet<QnUuid> QnAbstractPermissionsModel::filteredResources(Filter filter, const QSet<QnUuid>& source)
{
    QSet<QnUuid> result;
    for (const auto& resource : filteredResources(filter, qnResPool->getResources(source)))
        result << resource->getId();
    return result;
}

Qn::GlobalPermission QnAbstractPermissionsModel::accessPermission(Filter filter)
{
    switch (filter)
    {
    case QnAbstractPermissionsModel::CamerasFilter:
        return Qn::GlobalAccessAllCamerasPermission;
    case QnAbstractPermissionsModel::LayoutsFilter:
        return Qn::GlobalAccessAllLayoutsPermission;
    case QnAbstractPermissionsModel::ServersFilter:
        return Qn::GlobalAccessAllServersPermission;

    default:
        break;
    }
    NX_ASSERT(false);
    return Qn::NoGlobalPermissions;
}

QString QnAbstractPermissionsModel::accessPermissionText(Filter filter)
{
    switch (filter)
    {
    case QnAbstractPermissionsModel::CamerasFilter:
        return tr("All Cameras");
        break;
    case QnAbstractPermissionsModel::LayoutsFilter:
        return tr("All Global Layouts");
        break;
    case QnAbstractPermissionsModel::ServersFilter:
        return tr("All Servers");
        break;
    default:
        break;
    }
    NX_ASSERT(false);
    return QString();
}

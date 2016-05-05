#pragma once

#include <core/resource/resource_fwd.h>

class QnAbstractPermissionsModel: public QObject
{
    Q_OBJECT

public:
    enum Filter
    {
        CamerasFilter,
        LayoutsFilter,
        ServersFilter
    };

    QnAbstractPermissionsModel(QObject* parent);
    virtual ~QnAbstractPermissionsModel();

    virtual Qn::GlobalPermissions rawPermissions() const = 0;
    virtual void setRawPermissions(Qn::GlobalPermissions value) = 0;

    virtual QSet<QnUuid> accessibleResources() const = 0;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) = 0;

    static QList<QnAbstractPermissionsModel::Filter> allFilters();

    static QnResourceList filteredResources(Filter filter, const QnResourceList& source);
    static QSet<QnUuid> filteredResources(Filter filter, const QSet<QnUuid>& source);

    static Qn::GlobalPermission accessPermission(Filter filter);
    static QString accessPermissionText(Filter filter);
};
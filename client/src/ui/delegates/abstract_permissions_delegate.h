#pragma once

class QnAbstractPermissionsDelegate
{
public:
    QnAbstractPermissionsDelegate() {}
    virtual ~QnAbstractPermissionsDelegate() {}

    virtual Qn::GlobalPermissions permissions() const = 0;
    virtual void setPermissions(Qn::GlobalPermissions value) = 0;

    virtual QSet<QnUuid> accessibleResources() const = 0;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) = 0;
};
#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/models/abstract_permissions_model.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUserGroupSettingsModel : public QnAbstractPermissionsModel, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnAbstractPermissionsModel base_type;
public:
    QnUserGroupSettingsModel(QObject* parent = nullptr);
    virtual ~QnUserGroupSettingsModel();

    QnUuid userGroupId() const;
    void setUserGroupId(const QnUuid& value);

    virtual Qn::GlobalPermissions rawPermissions() const override;
    virtual void setRawPermissions(Qn::GlobalPermissions value) override;

    virtual QSet<QnUuid> accessibleResources() const override;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) override;

    QString groupName() const;

    /** Return human-readable permissions description for the target user */
    QString permissionsDescription() const;

    /** Return human-readable permissions description for the selected permissions set. */
    QString permissionsDescription(Qn::GlobalPermissions permissions, const QnUuid& groupId) const;

private:
    QString getCustomPermissionsDescription(const QnUuid &id, Qn::GlobalPermissions permissions) const;

private:
    QnUuid m_userGroupId;
};

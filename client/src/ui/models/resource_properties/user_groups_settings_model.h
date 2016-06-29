#pragma once

#include <nx_ec/data/api_user_group_data.h>

#include <ui/models/abstract_permissions_model.h>
#include <ui/workbench/workbench_context_aware.h>

/** Model for editing list of user groups */
class QnUserGroupSettingsModel : public QAbstractListModel, public QnAbstractPermissionsModel
{
    Q_OBJECT

    typedef QAbstractListModel base_type;
public:
    QnUserGroupSettingsModel(QObject* parent = nullptr);
    virtual ~QnUserGroupSettingsModel();

    ec2::ApiUserGroupDataList groups() const;
    void setGroups(const ec2::ApiUserGroupDataList& value);

    /** Add new group. Returns index of added group in the model. */
    int addGroup(const ec2::ApiUserGroupData& group);
    void removeGroup(const QnUuid& id);

    /** Select group as current. */
    void selectGroup(const QnUuid& value);
    QnUuid selectedGroup() const;

    /* Following  methods are working with the currently selected group. */

    QString groupName() const;
    void setGroupName(const QString& value);

    virtual Qn::GlobalPermissions rawPermissions() const override;
    virtual void setRawPermissions(Qn::GlobalPermissions value) override;

    virtual QSet<QnUuid> accessibleResources() const override;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) override;

    /** Get accessible resources for the given group - just for convenience. */
    QSet<QnUuid> accessibleResources(const QnUuid& groupId) const;

    /* Methods of QAbstractItemModel */

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;

private:
    ec2::ApiUserGroupDataList::iterator currentGroup();
    ec2::ApiUserGroupDataList::const_iterator currentGroup() const;

private:
    QnUuid m_currentGroupId;
    ec2::ApiUserGroupDataList m_groups;
    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
};

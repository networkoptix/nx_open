#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_access_rights_data.h>

#include <nx/utils/singleton.h>

class QnResourceAccessManager : public QObject, public Singleton<QnResourceAccessManager> {
    Q_OBJECT

public:
    QnResourceAccessManager(QObject* parent = nullptr);

    void reset(const ec2::ApiAccessRightsDataList& accessRights);
    ec2::ApiAccessRightsDataList allAccessRights() const;

    ec2::ApiAccessRightsData accessRights(const QnUuid& userId) const;
    void setAccessRights(const ec2::ApiAccessRightsData& accessRights);

    /**
    * \param user                      User to get global permissions for.
    * \returns                         Global permissions of the given user,
    *                                  adjusted to take deprecation and superuser status into account.
    */
    Qn::GlobalPermissions globalPermissions(const QnUserResourcePtr &user) const;


private:
    /**
    * \param permissions               Permission flags containing some deprecated values.
    * \returns                         Permission flags with deprecated values replaced with new ones.
    */
    static Qn::GlobalPermissions undeprecate(Qn::GlobalPermissions permissions);

private:
    ec2::ApiAccessRightsDataList m_values;
};

#define qnResourceAccessManager QnResourceAccessManager::instance()

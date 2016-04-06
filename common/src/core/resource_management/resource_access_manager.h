#pragma once

#include <QtCore/QObject>

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

private:
    ec2::ApiAccessRightsDataList m_values;
};

#define qnResourceAccessManager QnResourceAccessManager::instance()

#pragma once

#include <QtCore/QObject>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/singleton.h>

class QnResourceAccessManager : public QObject, public Singleton<QnResourceAccessManager> {
    Q_OBJECT

public:
    QnResourceAccessManager(QObject* parent = nullptr);

    void reset(const ec2::ApiAccessRightsDataList& accessRights);

private:

};

#define qnResourceAccessManager QnResourceAccessManager::instance()
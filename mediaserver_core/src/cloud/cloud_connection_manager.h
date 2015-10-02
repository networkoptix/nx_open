/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MS_CLOUD_CONNECTION_MANAGER_H
#define NX_MS_CLOUD_CONNECTION_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <utils/thread/mutex.h>
#include <utils/common/singleton.h>
#include <utils/common/safe_direct_connection.h>
#include <cdb/connection.h>


class CloudConnectionManager
:
    public QObject,
    public Singleton<CloudConnectionManager>,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT 

public:
    CloudConnectionManager();
    ~CloudConnectionManager();

    bool bindedToCloud() const;
    std::unique_ptr<nx::cdb::api::Connection> getCloudConnection();

signals:
    void cloudBindingStatusChanged(bool bindedToCloud);

private:
    QString m_cloudSystemID;
    QString m_cloudAuthKey;
    mutable QnMutex m_mutex;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> m_cdbConnectionFactory;

    bool bindedToCloud(QnMutexLockerBase* const lk) const;

private slots:
    void atResourceChanged(const QnResourcePtr& res);
};

#endif  //NX_MS_CLOUD_CONNECTION_MANAGER_H

/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cloud_connection_manager.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

#include "media_server/settings.h"


CloudConnectionManager::CloudConnectionManager()
:
    m_cdbConnectionFactory(createConnectionFactory(), destroyConnectionFactory)
{
    const auto cdbEndpoint = MSSettings::roSettings()->value(
        nx_ms_conf::CDB_ENDPOINT,
        "").toString();
    if (!cdbEndpoint.isEmpty())
    {
        const auto hostAndPort = cdbEndpoint.split(":");
        if (hostAndPort.size() == 2)
        {
            m_cdbConnectionFactory->setCloudEndpoint(
                hostAndPort[0].toStdString(),
                hostAndPort[1].toInt());
        }
    }

    Qn::directConnect(
        qnResPool, &QnResourcePool::resourceAdded,
        this, &CloudConnectionManager::atResourceAdded);
    if (const auto admin = qnResPool->getAdministrator())
        atResourceAdded(admin);
}

CloudConnectionManager::~CloudConnectionManager()
{
   directDisconnectAll();
}

bool CloudConnectionManager::bindedToCloud() const
{
    QnMutexLocker lk(&m_mutex);
    return bindedToCloud(&lk);
}

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection()
{
    QnMutexLocker lk(&m_mutex);
    if (!bindedToCloud(&lk))
        return nullptr;
    return m_cdbConnectionFactory->createConnection(
        m_cloudSystemID.toStdString(),
        m_cloudAuthKey.toStdString());
}

const nx::cdb::api::ConnectionFactory& CloudConnectionManager::connectionFactory() const
{
    return *m_cdbConnectionFactory;
}

bool CloudConnectionManager::bindedToCloud(QnMutexLockerBase* const /*lk*/) const
{
    return !m_cloudSystemID.isEmpty() && !m_cloudAuthKey.isEmpty();
}

void CloudConnectionManager::atResourceAdded(const QnResourcePtr& res)
{
    auto user = res.dynamicCast<QnUserResource>();
    if (!user || !user->isAdmin())
        return;

    Qn::directConnect(
        user.data(), &QnResource::propertyChanged,
        this, &CloudConnectionManager::atAdminPropertyChanged);
    atAdminPropertyChanged(user, QString());
}

void CloudConnectionManager::atAdminPropertyChanged(
    const QnResourcePtr& res,
    const QString& /*key*/)
{
    auto user = res.dynamicCast<QnUserResource>();
    NX_ASSERT(user);

    const auto cloudSystemID = user->getProperty(Qn::CLOUD_SYSTEM_ID);
    const auto cloudAuthKey = user->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY);

    QnMutexLocker lk(&m_mutex);
    if (cloudSystemID == m_cloudSystemID &&
        cloudAuthKey == m_cloudAuthKey)
    {
        return;
    }

    m_cloudSystemID = cloudSystemID;
    m_cloudAuthKey = cloudAuthKey;
    const bool bindedToCloud = !m_cloudSystemID.isEmpty() && !m_cloudAuthKey.isEmpty();

    lk.unlock();
    emit cloudBindingStatusChanged(bindedToCloud);
}

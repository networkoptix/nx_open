/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cloud_connection_manager.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/network/socket_global.h>

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

    Qn::directConnect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, this, &CloudConnectionManager::cloudSettingsChanged);
}

CloudConnectionManager::~CloudConnectionManager()
{
   directDisconnectAll();
}

boost::optional<nx::hpm::api::SystemCredentials>
    CloudConnectionManager::getSystemCredentials() const
{
    QnMutexLocker lk(&m_mutex);
    if (m_cloudSystemID.isEmpty() || m_cloudAuthKey.isEmpty())
        return boost::none;

    nx::hpm::api::SystemCredentials cloudCredentials;
    cloudCredentials.systemId = m_cloudSystemID.toUtf8();
    cloudCredentials.serverId = qnCommon->moduleGUID().toByteArray();
    cloudCredentials.key = m_cloudAuthKey.toUtf8();
    return cloudCredentials;
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

void CloudConnectionManager::cloudSettingsChanged()
{
    const auto cloudSystemId = qnGlobalSettings->cloudSystemID();
    const auto cloudAuthKey = qnGlobalSettings->cloudAuthKey();

    QnMutexLocker lk(&m_mutex);
    if (cloudSystemId == m_cloudSystemID &&
        cloudAuthKey == m_cloudAuthKey)
    {
        return;
    }

    m_cloudSystemID = cloudSystemId;
    m_cloudAuthKey = cloudAuthKey;
    const bool bindedToCloud = !m_cloudSystemID.isEmpty() && !m_cloudAuthKey.isEmpty();

    lk.unlock();

    QnModuleInformation info = qnCommon->moduleInformation();
    if (info.cloudSystemId != QnUuid::fromStringSafe(cloudSystemId))
    {
        info.cloudSystemId = QnUuid::fromStringSafe(cloudSystemId);
        qnCommon->setModuleInformation(info);
    }

    if (bindedToCloud)
    {
        nx::hpm::api::SystemCredentials credentials(
            cloudSystemId.toUtf8(),
            qnCommon->moduleGUID().toSimpleString().toUtf8(),
            cloudAuthKey.toUtf8());

        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(std::move(credentials));
    }
    else
    {
        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(boost::none);
    }

    emit cloudBindingStatusChanged(bindedToCloud);
}

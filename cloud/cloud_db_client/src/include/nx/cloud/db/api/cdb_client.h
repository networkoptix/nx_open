// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "connection.h"

namespace nx::cloud::db::api {

class CdbClient
{
public:
    CdbClient():
        m_connectionFactory(createConnectionFactory())
    {
    }

    ~CdbClient()
    {
        m_connection.reset();
        destroyConnectionFactory(m_connectionFactory);
    }

    void setCloudUrl(const std::string& url)
    {
        m_connectionFactory->setCloudUrl(url);
    }

    void setCredentials(
        const std::string& userName,
        const std::string& password)
    {
        m_userName = userName;
        m_password = password;
    }

    api::AccountManager* accountManager()
    {
        if (!createConnectionIfNeeded())
            return nullptr;
        return m_connection->accountManager();
    }

    api::SystemManager* systemManager()
    {
        if (!createConnectionIfNeeded())
            return nullptr;
        return m_connection->systemManager();
    }

    api::AuthProvider* authProvider()
    {
        if (!createConnectionIfNeeded())
            return nullptr;
        return m_connection->authProvider();
    }

    api::MaintenanceManager* maintenanceManager()
    {
        if (!createConnectionIfNeeded())
            return nullptr;
        return m_connection->maintenanceManager();
    }

private:
    nx::cloud::db::api::ConnectionFactory* m_connectionFactory;
    std::unique_ptr<api::Connection> m_connection;
    std::string m_userName;
    std::string m_password;

    bool createConnectionIfNeeded()
    {
        if (!m_connection)
        {
            m_connection = m_connectionFactory->createConnection(
                nx::network::http::PasswordCredentials(m_userName, m_password));
        }
        return m_connection != nullptr;
    }
};

} // namespace nx::cloud::db::api

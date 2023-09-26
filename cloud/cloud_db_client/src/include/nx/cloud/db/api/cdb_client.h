// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

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
        const std::string& username,
        const std::string& password)
    {
        m_credentials = nx::network::http::PasswordCredentials(username, password);
    }

    void setCredentials(nx::network::http::Credentials credentials)
    {
        m_credentials = std::move(credentials);
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

    void setRequestTimeout(std::chrono::milliseconds timeout)
    {
        m_requestTimeout = timeout;
        if (m_connection)
            m_connection->setRequestTimeout(timeout);
    }

private:
    nx::cloud::db::api::ConnectionFactory* m_connectionFactory;
    std::unique_ptr<api::Connection> m_connection;
    std::optional<nx::network::http::Credentials> m_credentials;
    std::optional<std::chrono::milliseconds> m_requestTimeout;

    bool createConnectionIfNeeded()
    {
        if (!m_connection)
        {
            if (m_credentials)
                m_connection = m_connectionFactory->createConnection(*m_credentials);
            else
                m_connection = m_connectionFactory->createConnection();

            if (m_connection && m_requestTimeout)
                m_connection->setRequestTimeout(*m_requestTimeout);
        }

        return m_connection != nullptr;
    }
};

} // namespace nx::cloud::db::api

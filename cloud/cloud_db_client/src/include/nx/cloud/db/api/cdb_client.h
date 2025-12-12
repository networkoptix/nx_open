// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include "connection.h"

namespace nx::cloud::db::api {

class CdbClient
{
public:
    CdbClient(int idleConnectionsLimit = 0):
        m_connection(nullptr),
        m_connectionFactory(createConnectionFactory(idleConnectionsLimit))
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

    void setIdleConnectionsLimit(int limit)
    {
        m_idleConnectionsLimit = limit;
    }

    void setCredentials(
        const std::string& username,
        const std::string& password)
    {
        m_credentials = nx::network::http::PasswordCredentials(username, password);
    }

    void setCredentials(nx::network::http::Credentials credentials)
    {
        m_credentials = credentials;
        if (m_connection)
            m_connection->setCredentials(std::move(credentials));
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

    api::OauthManager* oauthManager()
    {
        if (!createConnectionIfNeeded())
            return nullptr;
        return m_connection->oauthManager();
    }

    void setRequestTimeout(std::chrono::milliseconds timeout)
    {
        m_requestTimeout = timeout;
        if (m_connection)
            m_connection->setRequestTimeout(timeout);
    }

    void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
    {
        if (createConnectionIfNeeded())
            m_connection->bindToAioThread(aioThread);
    }

    nx::network::http::ClientOptions& httpClientOptions()
    {
        if (createConnectionIfNeeded())
        {
            return m_connection->httpClientOptions();
        }
        else
        {
            // TODO: #anekrasov this must be refactored to avoid throwing here
            NX_ERROR(this, "Connection to CDB cannot be created");
            throw std::runtime_error("Connection to CDB cannot be created");
        }
    }

private:
    std::unique_ptr<api::Connection> m_connection;
    nx::cloud::db::api::ConnectionFactory* m_connectionFactory;
    std::optional<nx::network::http::Credentials> m_credentials;
    std::optional<std::chrono::milliseconds> m_requestTimeout;
    int m_idleConnectionsLimit = 0;

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

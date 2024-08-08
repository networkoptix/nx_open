// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <future>
#include <memory>
#include <optional>

#include <QtCore/QObject>

#include <nx/cloud/db/api/oauth_data.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/helpers.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/system_context.h>

#include "logon_data.h"
#include "remote_connection_error.h"
#include "remote_connection_user_interaction_delegate.h"

namespace nx::vms::client::core {

class CertificateCache;

/**
 * Stores all the data received during the connection establishing process.
 */
struct NX_VMS_CLIENT_CORE_API RemoteConnectionFactoryContext: public QObject
{
    /** Whether connection process was terminated. */
    std::atomic_bool terminated = false;

    QPointer<SystemContext> systemContext;

    /** Initial data, describing where to connect and what to expect. */
    LogonData logonData;

    /** Id of the connection in the Audit Trail and Runtime Info Manager. */
    const nx::Uuid auditId = nx::Uuid::createUuid();

    struct CloudTokenInfo
    {
        nx::cloud::db::api::IssueTokenResponse response;
        std::optional<RemoteConnectionError> error;
    };
    /** Result of the Cloud token request. */
    std::future<CloudTokenInfo> cloudToken;

    std::optional<std::chrono::microseconds> sessionTokenExpirationTime;

    /** Information about the current Server. */
    nx::vms::api::ModuleInformation moduleInformation;

    /** List of all Servers of the System (5.0+). */
    std::vector<nx::vms::api::ServerInformationV1> serversInfo;

    /** User info for the pre-6.0 Systems where old permissions model is implemented. */
    std::optional<nx::vms::api::UserModelV1> compatibilityUserModel;

    nx::network::ssl::CertificateChain handshakeCertificateChain;
    std::shared_ptr<CertificateCache> certificateCache;
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> customUserInteractionDelegate;

    RemoteConnectionFactoryContext(SystemContext* systemContext): systemContext(systemContext) {}

    const nx::network::SocketAddress& address() const { return logonData.address; }
    const nx::network::http::Credentials& credentials() const { return logonData.credentials; }
    nx::vms::api::UserType userType() const { return logonData.userType; }
    std::optional<nx::Uuid> expectedServerId() const { return logonData.expectedServerId; }

    std::optional<nx::utils::SoftwareVersion> expectedServerVersion() const
    {
        return logonData.expectedServerVersion;
    }

    std::optional<QString> expectedCloudSystemId() const
    {
        return logonData.expectedCloudSystemId;
    }

    bool isCloudConnection() const { return userType() == nx::vms::api::UserType::cloud; }

    bool failed() const { return m_error.has_value(); }

    std::optional<RemoteConnectionError> error() const { return m_error; }

    /**
     * Set initial error. Calling setError when error is already set may hide the original problem,
     * so it is forbidden. Use rewriteError if this needs to be done.
     */
    void setError(std::optional<RemoteConnectionError> value)
    {
        if (NX_ASSERT(!failed(), "Context already has error %1", m_error->code))
        {
            NX_DEBUG(this, "Set error %1", *value);
            m_error = value;
        }
    }

    /**
     * Forcefully override error with a new value. Used to detalize some situations for the user.
     */
    void rewriteError(RemoteConnectionError value) { m_error = value; }

    /** Forcefully clear error. */
    void resetError() { m_error = {}; }

    QString toString() const;

    bool isRestApiSupported() const;

private:
    std::optional<RemoteConnectionError> m_error;
};

struct RemoteConnectionProcess
{
    std::shared_ptr<RemoteConnectionFactoryContext> context;
    std::future<void> future;

    RemoteConnectionProcess(SystemContext* systemContext);
    ~RemoteConnectionProcess();
};

} // namespace nx::vms::client::core

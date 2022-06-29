// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <future>
#include <memory>
#include <optional>

#include <QtCore/QObject>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/helpers.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

#include "connection_info.h"
#include "remote_connection_error.h"
#include "remote_connection_user_interaction_delegate.h"

namespace nx::vms::client::core {

class CertificateCache;

struct RemoteConnectionFactoryContext: public QObject
{
    using Purpose = common::ServerCompatibilityValidator::Purpose;

    ConnectionInfo info;
    Purpose purpose = Purpose::connect;
    std::optional<std::chrono::microseconds> sessionTokenExpirationTime;
    nx::vms::api::ModuleInformation moduleInformation;
    nx::network::ssl::CertificateChain certificateChain;
    bool targetHasUserProvidedCertificate = false;
    std::shared_ptr<CertificateCache> certificateCache;
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> customUserInteractionDelegate;

    std::optional<RemoteConnectionError> error;

    bool failed() const { return error.has_value(); }

    QString toString() const;
};

struct RemoteConnectionProcess
{
    std::shared_ptr<RemoteConnectionFactoryContext> context;
    std::future<void> future;

    RemoteConnectionProcess():
        context(new RemoteConnectionFactoryContext())
    {
    }
};

} // namespace nx::vms::client::core

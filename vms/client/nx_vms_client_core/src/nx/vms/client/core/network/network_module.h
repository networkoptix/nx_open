// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/fusion/serialization_format.h>
#include <nx/utils/impl_ptr.h>

#include "server_certificate_validation_level.h"

class QnCommonModule;
namespace nx::vms::api { enum class PeerType; }

namespace nx::vms::client::core {

class RemoteSession;
class CertificateVerifier;
class RemoteConnectionFactory;
class RemoteSessionTimeoutWatcher;

/**
 * Single storage place for all network-related classes intances in the client core. Maintains their
 * lifetime, internal dependencies and construction / destruction order.
 */
class NetworkModule: public QObject
{
    Q_OBJECT

public:
    NetworkModule(
        QnCommonModule* commonmModule,
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat,
        ServerCertificateValidationLevel certificateValidationLevel);
    virtual ~NetworkModule();

    RemoteConnectionFactory* connectionFactory() const;

    /**
     * Client-side certificate verifier.
     */
    CertificateVerifier* certificateVerifier() const;

    RemoteSessionTimeoutWatcher* sessionTimeoutWatcher() const;

    std::shared_ptr<RemoteSession> session() const;
    void setSession(std::shared_ptr<RemoteSession> session);

    void reinitializeCertificateStorage(
        ServerCertificateValidationLevel certificateValidationLevel);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

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
class NX_VMS_CLIENT_CORE_API NetworkModule: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    NetworkModule(
        SystemContext* systemContext,
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat);
    virtual ~NetworkModule();

    RemoteConnectionFactory* connectionFactory() const;

    /**
     * Client-side certificate verifier.
     */
    CertificateVerifier* certificateVerifier() const;

    RemoteSessionTimeoutWatcher* sessionTimeoutWatcher() const;

    std::shared_ptr<RemoteSession> session() const;
    void setSession(std::shared_ptr<RemoteSession> session);
    QnUuid currentServerId() const;

    void reinitializeCertificateStorage();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core

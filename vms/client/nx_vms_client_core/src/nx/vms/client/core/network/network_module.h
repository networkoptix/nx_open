// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class RemoteConnectionFactory;
class CertificateVerifier;
class CloudConnectionFactory;

/**
 * Single storage place for all network-related classes instances in the client core.
 * Maintains their lifetime, internal dependencies and construction / destruction order.
 */
class NX_VMS_CLIENT_CORE_API NetworkModule: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    NetworkModule(QObject* parent = nullptr);
    virtual ~NetworkModule();

    void reinitializeCertificateStorage();

    RemoteConnectionFactory* connectionFactory() const;

    CertificateVerifier* certificateVerifier() const;

    CloudConnectionFactory* cloudConnectionFactory() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core

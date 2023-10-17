// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

class CertificateVerifier;

/**
 * Caches server certificates as soon as server is added to the resources pool.
 * Watches for their changes and updates cached information if needed.
 */
class ServerCertificateWatcher: public QObject, public SystemContextAware
{
    using base_type = QObject;

public:
    ServerCertificateWatcher(
        SystemContext* systemContext,
        CertificateVerifier* certificateVerifier,
        QObject* parent = nullptr);
};

} // namespace nx::vms::client::core

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "remote_connection_fwd.h"

namespace nx::vms::client::core {

/**
 * Terminates local user authorization token. Waits in destructor for 3 seconds if some tokens
 *     are still not removed.
 */
class SessionTokenTerminator: public QObject {
public:
    SessionTokenTerminator(QObject* parent = nullptr);
    virtual ~SessionTokenTerminator() override;

    void terminateToken(RemoteConnectionPtr connection);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core

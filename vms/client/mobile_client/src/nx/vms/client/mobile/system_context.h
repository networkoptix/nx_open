// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context.h>

Q_MOC_INCLUDE("nx/vms/client/mobile/session/session_manager.h")

namespace nx::vms::client::mobile {

class MediaDownloadManager;
class SessionManager;

class SystemContext: public core::SystemContext
{
    Q_OBJECT

    using base_type = core::SystemContext;

    Q_PROPERTY(SessionManager* sessionManager
        READ sessionManager
        CONSTANT)

public:
    /**
     * @see nx::vms::client::core::SystemContext
     */
    SystemContext(Mode mode, nx::Uuid peerId, QObject* parent = nullptr);

    virtual ~SystemContext() override;

    SessionManager* sessionManager() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/network/ssl/helpers.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::api { struct ModuleInformation; }
namespace nx::network { class SocketAddress; }

namespace nx::vms::client::desktop {

class ServerCertificateError: public QnMessageBox
{
    Q_OBJECT
    using base_type = QnMessageBox;

public:
    explicit ServerCertificateError(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& certificates,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop

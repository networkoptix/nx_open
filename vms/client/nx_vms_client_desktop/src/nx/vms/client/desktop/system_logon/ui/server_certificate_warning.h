// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/network/ssl/helpers.h>
#include <nx/vms/client/core/system_logon/certificate_warning.h>
#include <ui/dialogs/common/message_box.h>

namespace nx::vms::api { struct ModuleInformation; }
namespace nx::vms::client::core { struct TargetCertificateInfo; }
namespace nx::network { class SocketAddress; }

namespace nx::vms::client::desktop {

class ServerCertificateWarning: public QnMessageBox
{
    Q_OBJECT
    using base_type = QnMessageBox;

public:
    explicit ServerCertificateWarning(
        const QList<core::TargetCertificateInfo>& certificatesInfo,
        core::CertificateWarning::Reason reason,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::utils { class SoftwareVersion; }
namespace nx::vms::api { struct ModuleInformation; }

namespace nx::vms::client::core {

enum class RemoteConnectionErrorCode;

struct NX_VMS_CLIENT_CORE_API RemoteConnectionErrorDescription
{
    /** Short one-line description, used on WS tiles or in mobile client. */
    QString shortText;

    /** Full description, used in the Login dialog and in the "Test connection" dialog. */
    QString longText;
};

NX_VMS_CLIENT_CORE_API RemoteConnectionErrorDescription errorDescription(
    RemoteConnectionErrorCode code,
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::utils::SoftwareVersion& engineVersion);

/** Short error description does not require client and server versions.  */
NX_VMS_CLIENT_CORE_API QString shortErrorDescription(RemoteConnectionErrorCode code);

} // namespace nx::vms::client::core

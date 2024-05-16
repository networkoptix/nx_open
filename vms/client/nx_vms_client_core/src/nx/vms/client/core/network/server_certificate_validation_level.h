// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core::network::server_certificate {

Q_NAMESPACE_EXPORT(NX_VMS_CLIENT_CORE_API)

NX_REFLECTION_ENUM_CLASS(ValidationLevel,
    disabled,
    recommended,
    strict
)

Q_ENUM_NS(ValidationLevel)

} // namespace nx::vms::client::core::network::server_certificate

Q_DECLARE_METATYPE(nx::vms::client::core::network::server_certificate::ValidationLevel)

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core {

NX_REFLECTION_ENUM_CLASS(ServerCertificateValidationLevel,
    disabled,
    recommended,
    strict
)

} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::ServerCertificateValidationLevel)

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

#include "../globals.h"

namespace nx::vms::client::desktop::jsapi {

/**
 * Represents the error description of some operation.
 * @ingroup vms
 */
struct Error
{
    using ErrorCode = Globals::ErrorCode;

    /** Error code of the operation. */
    int code = ErrorCode::success;

    /** Human-readable description of the error. */
    QString description;

    /** @privatesection */

    static Error make(Globals::ErrorCode code,
        const QString& description = QString());

    static Error success();
    static Error denied();
    static Error failed(const QString& description = QString());
    static Error invalidArguments(const QString& description = QString());
};
NX_REFLECTION_INSTRUMENT(Error, (code)(description))

} // namespace nx::vms::client::desktop::jsapi

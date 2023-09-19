// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

#include "../globals.h"

namespace nx::vms::client::desktop::jsapi {

/** Represents the error description of some operation. */
struct Error
{
    int code = Globals::success;
    QString description;

    static Error make(Globals::ErrorCode code,
        const QString& description = QString());

    static Error success();
    static Error denied();
    static Error failed(const QString& description = QString());
    static Error invalidArguments(const QString& description = QString());
};
NX_REFLECTION_INSTRUMENT(Error, (code)(description))

} // namespace nx::vms::client::desktop::jsapi

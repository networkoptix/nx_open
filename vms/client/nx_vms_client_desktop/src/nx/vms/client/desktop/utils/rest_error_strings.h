// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

class RestErrorStrings
{
    Q_DECLARE_TR_FUNCTIONS(RestErrorStrings)

public:
    template<typename T>
    static QString description(const T& error)
    {
        if (!error.errorString.isEmpty())
            return error.errorString;

        return tr("Error code: %1").arg((int) error.errorId);
    }
};

} // namespace nx::vms::client::desktop

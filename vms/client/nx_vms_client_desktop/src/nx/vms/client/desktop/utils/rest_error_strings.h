// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QLocale>

#include <nx/branding.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

class RestErrorStrings
{
    Q_DECLARE_TR_FUNCTIONS(RestErrorStrings)

public:
    // We cannot guarantee that errorString is translated, so we show it only in the developer mode
    // or for English locale.
    template<typename T>
    static QString description(const T& error)
    {
        auto languageName = appContext()->coreSettings()->locale().split('_').first();
        if (languageName.isEmpty())
            languageName = nx::branding::defaultLocale().split('_').first();

        return ini().developerMode || languageName == "en"
            ? getPersistentStringPart((int)error.errorId) + ". " + error.errorString
            : getPersistentStringPart((int)error.errorId);
    }

private:
    static QString getPersistentStringPart(int errorId);
};

} // namespace nx::vms::client::desktop

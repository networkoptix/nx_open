// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screens.h"

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>

#include "qml_result_watcher.h"

namespace nx::vms::client::mobile::detail {

namespace {

static const auto kLoginScreenUrl = QUrl("qrc:///qml/Nx/Screens/Cloud/Login.qml");

} // namespace

Screens::Screens(WindowContext* context, QObject* parent):
    base_type(parent),
    WindowContextAware(context)
{
}

void Screens::open2faValidationScreen(
    const network::http::Credentials& credentials, std::promise<bool> result) const
{
    const QVariantMap properties{
        {"user", QString::fromStdString(credentials.username)},
        {"token", QString::fromStdString(credentials.authToken.value)},
        {"twoFactorValidation", true},
    };

    const auto screen = QmlWrapperHelper::pushScreen(windowContext(), kLoginScreenUrl, properties);
    if (!screen)
    {
        result.set_value(false);
        return;
    }

    // The watcher emits the result exactly once, also when the screen is destroyed without it.
    const auto watcher = new QmlResultWatcher(screen);
    connect(watcher,
        &QmlResultWatcher::resultReady,
        watcher,
        [result = std::move(result)](const QString& value) mutable
        { result.set_value(value == "success"); });
}

QmlResultWatcher* Screens::openCloudLoginScreen(
    bool reauthentication, const QString& user, const QString& token) const
{
    QVariantMap properties{
        {"forced", reauthentication},
        {"user", user},
    };
    if (!token.isEmpty())
        properties["token"] = token;

    const auto screen = QmlWrapperHelper::pushScreen(windowContext(), kLoginScreenUrl, properties);
    return screen ? new QmlResultWatcher(screen) : nullptr;
}

} // namespace nx::vms::client::mobile::detail

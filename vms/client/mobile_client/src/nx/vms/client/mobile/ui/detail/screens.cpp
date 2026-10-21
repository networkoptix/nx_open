// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screens.h"

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>

#include "qml_result_watcher.h"

namespace nx::vms::client::mobile::detail {

namespace {

static const auto kLoginScreenUrl = QUrl("qrc:///qml/Nx/Screens/Cloud/Login.qml");

QVariantMap cloudLoginProperties(bool reauthentication, const QString& user, const QString& token)
{
    QVariantMap properties;
    properties["forced"] = reauthentication;
    properties["user"] = user;
    if (!token.isEmpty())
        properties["token"] = token;
    return properties;
}

} // namespace

Screens::Screens(WindowContext* context, QObject* parent):
    base_type(parent),
    WindowContextAware(context)
{
}

bool Screens::show2faValidationScreen(const network::http::Credentials& credentials) const
{
    QVariantMap properties;
    properties["user"] = QString::fromStdString(credentials.username);
    properties["token"] = QString::fromStdString(credentials.authToken.value);
    properties["twoFactorValidation"] = true;
    return QmlWrapperHelper::showScreen(windowContext(), kLoginScreenUrl, properties) == "success";
}

QmlResultWatcher* Screens::openCloudLoginScreen(
    bool reauthentication, const QString& user, const QString& token) const
{
    const auto screen = QmlWrapperHelper::pushScreen(
        windowContext(), kLoginScreenUrl, cloudLoginProperties(reauthentication, user, token));

    return screen ? new QmlResultWatcher(screen) : nullptr;
}

} // namespace nx::vms::client::mobile::detail

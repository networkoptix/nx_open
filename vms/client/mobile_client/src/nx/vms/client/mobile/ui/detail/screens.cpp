// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screens.h"

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>

namespace nx::vms::client::mobile::detail {

Screens::Screens(WindowContext* context, QObject* parent):
    base_type(parent),
    WindowContextAware(context)
{
}

bool Screens::show2faValidationScreen(const network::http::Credentials& credentials) const
{
    static const auto kLoginScreenUrl = QUrl("qrc:qml/Nx/Screens/Cloud/Login.qml");

    QVariantMap properties;
    properties["token"] = QString::fromStdString(credentials.authToken.value);
    return QmlWrapperHelper::showScreen(windowContext(), kLoginScreenUrl, properties) == "success";
}

} // namespace nx::vms::client::mobile::detail

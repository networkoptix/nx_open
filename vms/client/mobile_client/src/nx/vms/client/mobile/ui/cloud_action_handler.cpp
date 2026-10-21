// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_action_handler.h"

#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <nx/vms/client/mobile/window_context.h>

#include "detail/qml_result_watcher.h"
#include "detail/screens.h"

namespace nx::vms::client::mobile {

CloudActionHandler::CloudActionHandler(WindowContext* context, QObject* parent):
    base_type(parent),
    WindowContextAware(context)
{
}

CloudActionHandler::CloudActionHandler(QObject* parent):
    base_type(parent),
    WindowContextAware(WindowContext::fromQmlContext(this))
{
}

CloudActionHandler::~CloudActionHandler()
{
}

void CloudActionHandler::requestCloudReauthorization(
    QObject* context, ReauthorizationHandler handler)
{
    NX_CRITICAL(context);

    // The main session and several cross-site contexts may request the authorization at once,
    // they share the screen.
    if (!m_cloudAuthScreenResult || m_cloudAuthScreenResult->hasResult())
    {
        m_cloudAuthScreenResult = openCloudLoginScreen();
        if (!m_cloudAuthScreenResult)
        {
            handler(core::OauthDataRequestError::noParent);
            return;
        }
    }

    connect(m_cloudAuthScreenResult,
        &detail::QmlResultWatcher::resultReady,
        context,
        [handler = std::move(handler)](const QString& result)
        {
            if (result == "success")
                handler(std::nullopt);
            else if (result == "failed")
                handler(core::OauthDataRequestError::internalCancellation);
            else
                handler(core::OauthDataRequestError::dialogRejected);
        });
}

void CloudActionHandler::handleCloudAuthRequest(core::CloudAuthRequest* request)
{
    request->start();
    requestCloudReauthorization(request,
        [request](std::optional<core::OauthDataRequestError> error) { request->finish(error); });
}

detail::QmlResultWatcher* CloudActionHandler::openCloudLoginScreen()
{
    const auto watcher = appContext()->cloudStatusWatcher();
    const auto token = QString::fromStdString(watcher->credentials().authToken.value);
    return uiController()->screens()->openCloudLoginScreen(
        /*reauthentication*/ true, watcher->cloudLogin(), token);
}

} // namespace nx::vms::client::mobile

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_action_handler.h"

#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>

#include "detail/qml_result_watcher.h"
#include "detail/screens.h"

namespace nx::vms::client::mobile {

CloudActionHandler::CloudActionHandler(WindowContext* context, QObject* parent):
    base_type(parent),
    WindowContextAware(context)
{
}

CloudActionHandler::~CloudActionHandler()
{
}

void CloudActionHandler::handleCloudAuthRequest(core::CloudAuthRequest* request)
{
    request->start();

    // Several cross-site contexts may request the authorization at once, they share the screen.
    if (!m_cloudAuthScreenResult || m_cloudAuthScreenResult->hasResult())
    {
        const auto watcher = appContext()->cloudStatusWatcher();
        const auto token = QString::fromStdString(watcher->credentials().authToken.value);
        m_cloudAuthScreenResult = uiController()->screens()->openCloudLoginScreen(
            /*reauthentication*/ true, /*user*/ {}, token);

        if (!m_cloudAuthScreenResult)
        {
            request->finish(core::OauthDataRequestError::noParent);
            return;
        }
    }

    connect(m_cloudAuthScreenResult,
        &detail::QmlResultWatcher::resultReady,
        request,
        [request](const QString& result)
        {
            if (result == "success")
                request->finish();
            else if (result == "failed")
                request->finish(core::OauthDataRequestError::internalCancellation);
            else
                request->finish(core::OauthDataRequestError::dialogRejected);
        });
}

} // namespace nx::vms::client::mobile

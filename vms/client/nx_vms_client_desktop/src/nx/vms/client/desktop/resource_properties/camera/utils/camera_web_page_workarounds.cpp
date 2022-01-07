// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_web_page_workarounds.h"

#include <QtWebEngineCore/QWebEngineScript>

#include <nx/utils/log/format.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>

namespace nx::vms::client::desktop {

namespace {

static const QString overrideXmlHttpRequestScriptName = "overrideXmlHttpRequestTimeout";

} // namespace

void CameraWebPageWorkarounds::setXmlHttpRequestTimeout(
    WebViewController* controller,
    std::chrono::milliseconds timeout)
{
    // Inject script that overrides the timeout before sending the XMLHttpRequest.
    QWebEngineScript script;
    const auto s = nx::format(R"JS(
        XMLHttpRequest.prototype.realSend = XMLHttpRequest.prototype.send;
        XMLHttpRequest.prototype.send = function(data) {
          if (this.timeout !== 0 && this.timeout < %1) {
            this.timeout = %1;
          }
          this.realSend(data);
        };
        )JS").arg(timeout.count());

    script.setName(overrideXmlHttpRequestScriptName);
    script.setSourceCode(s);
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setRunsOnSubFrames(true);
    script.setWorldId(QWebEngineScript::MainWorld);

    controller->addProfileScript(script);
}

} // namespace nx::vms::client::desktop

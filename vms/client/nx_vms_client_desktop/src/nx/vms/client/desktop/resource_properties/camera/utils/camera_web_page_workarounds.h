// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

namespace nx::vms::client::desktop {

class WebViewController;

/**
 * Collection of workarounds for proxying web pages via vms servers.
 */
class NX_VMS_CLIENT_DESKTOP_API CameraWebPageWorkarounds
{
public:
    /** Override the timeout before sending the XMLHttpRequest. */
    static void setXmlHttpRequestTimeout(
        WebViewController* controller,
        std::chrono::milliseconds timeout);

private:
    CameraWebPageWorkarounds();
};

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/common/update/update_check_params.h>
#include <nx/vms/common/update/update_information.h>

#include "update_contents.h"

namespace nx::vms::client::desktop::system_update {

rest::Handle requestUpdateInformation(
    const rest::ServerConnectionPtr& connection,
    const common::update::UpdateInfoParams& params,
    rest::JsonResultCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor = {});

rest::Handle requestUpdateStatus(
    const rest::ServerConnectionPtr& connection,
    rest::JsonResultCallback&& callback,
    nx::utils::AsyncHandlerExecutor executor = {});

/** Get update info directly from the Internet or via the given proxy connection. */
UpdateContents getUpdateContents(
    rest::ServerConnectionPtr proxyConnection,
    const nx::Url& url,
    const nx::vms::common::update::UpdateInfoParams& params,
    const bool skipVersionEqualToCurrent = true);

} // namespace nx::vms::client::desktop::system_update

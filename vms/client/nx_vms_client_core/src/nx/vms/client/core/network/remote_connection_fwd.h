// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

namespace nx::vms::client::core {

class RemoteConnection;
using RemoteConnectionPtr = std::shared_ptr<RemoteConnection>;
class RemoteSession;
enum class RemoteConnectionErrorCode;

} // namespace nx::vms::client::core

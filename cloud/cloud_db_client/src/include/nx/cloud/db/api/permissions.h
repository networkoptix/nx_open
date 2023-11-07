// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::cloud::db::api {

namespace roles {

static constexpr char kAdministrator[] = "00000000-0000-0000-0000-100000000000";
static constexpr char kPowerUser[] = "00000000-0000-0000-0000-100000000001";
static constexpr char kAdvancedViewer[] = "00000000-0000-0000-0000-100000000002";
static constexpr char kViewer[] = "00000000-0000-0000-0000-100000000003";
static constexpr char kLiveViewer[] = "00000000-0000-0000-0000-100000000004";
static constexpr char kSystemHealthViewer[] = "00000000-0000-0000-0000-100000000005";

} // namespace roles

} // namespace nx::cloud::db::api

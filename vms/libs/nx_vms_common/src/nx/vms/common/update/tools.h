// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/software_version.h>
#include <nx/utils/url.h>

namespace nx::vms::common { class SystemSettings; }

namespace nx::vms::common::update {

NX_VMS_COMMON_API nx::Url releaseListUrl(SystemSettings* globalSettings);
NX_VMS_COMMON_API nx::Url updateGeneratorUrl();
NX_VMS_COMMON_API QString passwordForBuild(const QString& build);

} // namespace nx::vms::common::update

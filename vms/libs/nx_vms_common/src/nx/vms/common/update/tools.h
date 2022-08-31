// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/software_version.h>

namespace nx::vms::common::update {

NX_VMS_COMMON_API QString updateFeedUrl();
NX_VMS_COMMON_API QString updateGeneratorUrl();
NX_VMS_COMMON_API QString passwordForBuild(const QString& build);

} // namespace nx::vms::common::update

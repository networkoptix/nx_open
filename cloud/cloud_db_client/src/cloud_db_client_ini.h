// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::cloud::db::api {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("cloud_db_client.ini") { reload(); }

    NX_INI_STRING("", customizedAnalyticsDbUrl,
        "Overrides the current analytycs DB service url");
};

Ini& ini();

} // nx::cloud::db::api

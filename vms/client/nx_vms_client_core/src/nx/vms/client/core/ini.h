// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::vms::client::core {

struct Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_vms_client_core.ini") { reload(); }

    // VMS-20420
    NX_INI_FLAG(true, bearerAuthentication,
        "[Feature] Enable bearer token authentication.");

    // VMS-30347.
    NX_INI_INT(8, maxLastConnectedTilesStored,
        "[Support] Maximum last connected systems tiles stored on the Welcome Screen");

    NX_INI_INT(0, systemsHideOptions,
        "[Dev] Hide systems, bitwise combination of flags:\n"
        " * 1 - Incompatible systems.\n"
        " * 2 - Not connectable cloud systems.\n"
        " * 4 - Compatible systems which require compatibility mode.\n"
        " * 8 - All systems except those on localhost.\n"
    );

    NX_INI_STRING("", rootCertificatesFolder,
        "Path to a folder with user-provided certificates. If set, all *.pem and *.crt files\n"
        "from this folder are used as trusted root certificates together with the system ones.");
};

Ini& ini();

} // namespace nx::vms::client::core

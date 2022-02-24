// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "client_process_execution_interface.h"

namespace nx::vms::client::desktop {

class ClientProcessRunner: public ClientProcessExecutionInterface
{
public:
    virtual PidType currentProcessPid() const override;
    virtual bool isProcessRunning(PidType pid) const override;

    virtual PidType runClient(const QStringList& arguments) const override;
    virtual PidType runClient(const StartupParameters& parameters) const override;
};

} // namespace nx::vms::client::desktop

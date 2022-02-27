// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QtGlobal>
#include <QtCore/QStringList>

namespace nx::vms::client::desktop {

struct StartupParameters;

class ClientProcessExecutionInterface
{
public:
    using PidType = quint64;

    virtual ~ClientProcessExecutionInterface() {}

    virtual PidType currentProcessPid() const = 0;
    virtual bool isProcessRunning(PidType pid) const = 0;

    virtual PidType runClient(const QStringList& arguments) const = 0;
    virtual PidType runClient(const StartupParameters& parameters) const = 0;
};
using ClientProcessExecutionInterfacePtr = std::unique_ptr<ClientProcessExecutionInterface>;

} // namespace nx::vms::client::desktop

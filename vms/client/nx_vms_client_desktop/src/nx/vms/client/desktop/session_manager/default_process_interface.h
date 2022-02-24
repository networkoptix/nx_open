// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/utils/platform/process.h>

#include "session_manager.h"

namespace nx::vms::client::desktop::session {

/**
 * Default implementation for session::ProcessInterface
 * NOTE: Another implementation is used inside tests.
 */
class DefaultProcessInterface: public ProcessInterface
{
public:
    virtual qint64 getOwnPid() const override
    {
        return QCoreApplication::applicationPid();
    }

    virtual bool checkProcessExists(qint64 pid) const override
    {
        return nx::utils::checkProcessExists(pid);
    }
};

} // namespace nx::vms::client::desktop::session

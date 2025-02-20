// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/qt_direct_connect.h>

namespace Qn {

class SafeDirectConnectionGlobalHelper;

/**
 * Class that want to receive signals directly, MUST inherit this type.
 * WARNING: Implementation MUST call EnableSafeDirectConnection::directDisconnectAll before object destruction.
 */
class NX_UTILS_API EnableSafeDirectConnection
{
public:
    EnableSafeDirectConnection() = default;

    /**
     * Disconnects from all directly-connected slot connected with directConnect.
     */
    void directDisconnectAll() { m_qtSignalGuards.clear(); }

    template<typename Object, typename Signal, typename Handler>
    void directConnect(const Object& object, Signal signal, Handler handler)
    {
        m_qtSignalGuards.emplace_back(nx::qtDirectConnect(object, signal, handler));
    }

private:
    std::vector<nx::utils::SharedGuardPtr> m_qtSignalGuards;
};

} // namespace Qn

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUuid>

#include <nx/reflect/instrument.h>

namespace nx::vms::client::mobile {

struct RemoteLogSessionData
{
    qint64 endTimeMs; //< End time of remote loggin session, in milliseconds since epoch.
    std::string sessionId;
};
NX_REFLECTION_INSTRUMENT(RemoteLogSessionData, (endTimeMs)(sessionId))

} // namespace nx::vms::client::mobile

Q_DECLARE_METATYPE(nx::vms::client::mobile::RemoteLogSessionData)

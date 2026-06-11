// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_data_packet.h"

#include <nx/utils/log/format.h>

QString QnAbstractDataPacket::idForToStringFromPtr() const
{
    return NX_FMT("timestamp: %1ms", timestamp / 1000);
}
